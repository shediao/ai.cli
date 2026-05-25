#include <curl/curl.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>  // For std::remove
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>  // For std::runtime_error
#include <string>

#ifdef _WIN32
#include <objbase.h>
#include <windows.h>  // For GetEnvironmentVariable
#else
#include <termios.h>
#include <unistd.h>  // For access, isatty, STDIN_FILENO/STDOUT_FILENO/STDERR_FILENO
#endif

#include <environment/environment.hpp>
#include <subprocess/subprocess.hpp>

#include "ai/utils.h"
#include "utfx/utfx.hpp"

namespace fs = std::filesystem;

namespace ai::utils {
#if defined(_WIN32)
namespace {
// Helper function to convert a UTF-8 std::string to a UTF-16 std::wstring
inline std::wstring to_wstring(const std::string& utf8str,
                               const UINT from_codepage = CP_UTF8) {
  if (utf8str.empty()) {
    return {};
  }
  int size_needed = MultiByteToWideChar(from_codepage, 0, utf8str.data(),
                                        (int)utf8str.size(), NULL, 0);
  if (size_needed <= 0) {
    // Consider throwing an exception for conversion errors
    return {};
  }
  std::wstring utf16str(size_needed, 0);
  MultiByteToWideChar(from_codepage, 0, utf8str.data(), (int)utf8str.size(),
                      utf16str.data(), size_needed);
  return utf16str;
}
// Helper function to convert a UTF-16 std::wstring to a UTF-8 std::string
inline std::string to_string(const std::wstring& utf16str,
                             const UINT to_codepage = CP_UTF8) {
  if (utf16str.empty()) {
    return {};
  }
  int size_needed =
      WideCharToMultiByte(to_codepage, 0, utf16str.data(), (int)utf16str.size(),
                          NULL, 0, NULL, NULL);
  if (size_needed <= 0) {
    // Consider throwing an exception for conversion errors
    return {};
  }
  std::string utf8str(size_needed, 0);
  WideCharToMultiByte(to_codepage, 0, utf16str.data(), (int)utf16str.size(),
                      utf8str.data(), size_needed, NULL, NULL);
  return utf8str;
}

}  // namespace
std::optional<std::string> toUtf8(const std::string& s) {
  auto u8 = to_string(to_wstring(s, GetACP()), CP_UTF8);
  if (!u8.empty()) {
    return u8;
  }
  return std::nullopt;
}
#endif

// libcurl write callback: receives data and writes it to an open file stream.
// ptr:   pointer to the received data block
// size:  size of each data element (typically 1)
// nmemb: number of data elements
// stream: user-defined pointer (a std::ofstream* in this case)
static size_t write_data_to_file(void* ptr, size_t size, size_t nmemb,
                                 void* stream) {
  std::ofstream* out_file = static_cast<std::ofstream*>(stream);
  if (out_file && out_file->is_open()) {
    out_file->write(static_cast<char*>(ptr), size * nmemb);
    if (out_file->fail()) {
      // Write failed — returning 0 makes libcurl abort with CURLE_WRITE_ERROR
      return 0;
    }
    return size * nmemb;  // Return the number of bytes successfully written
  }
  return 0;  // Invalid file stream — abort transfer
}

bool download_image(std::string const& image_url, std::string const& image_path,
                    std::string& mime_type, std::string const& proxy) {
  CURL* curl_handle;
  CURLcode res;
  std::ofstream outfile;

  // 1. Obtain a CURL easy handle
  curl_handle = curl_easy_init();
  if (!curl_handle) {
    std::cerr << "Error: curl_easy_init() failed." << std::endl;
    return false;
  }

  // 2. Open local file for writing (binary mode)
  outfile.open(image_path, std::ios::binary);
  if (!outfile.is_open()) {
    std::cerr << "Error: Cannot open file for writing: " << image_path
              << std::endl;
    curl_easy_cleanup(curl_handle);
    return false;
  }

  // 3. Set libcurl options
  // Set the URL to download
  curl_easy_setopt(curl_handle, CURLOPT_URL, image_url.c_str());

  // Set the write callback function
  curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data_to_file);

  // Pass the file stream pointer to the callback
  curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &outfile);

  // Follow HTTP 3xx redirects
  curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);

  // Treat HTTP 4xx/5xx as errors (don't download the error page)
  curl_easy_setopt(curl_handle, CURLOPT_FAILONERROR, 1L);

  // Set timeouts
  curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT_MS, 60000L);  // 60s total
  curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT_MS,
                   3000L);  // 3s connect timeout

  if (!proxy.empty()) {
    curl_easy_setopt(curl_handle, CURLOPT_PROXY, proxy.c_str());
  }

  // (Optional) verbose output for debugging
  // curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 1L);

  // 4. Perform the transfer
  res = curl_easy_perform(curl_handle);

  // 5. Close the file stream (ensure all data is flushed to disk)
  outfile.close();
  // 6. Check the result
  if (res != CURLE_OK) {
    std::cerr << "Error: curl_easy_perform() failed: "
              << curl_easy_strerror(res) << std::endl;
    // Download failed — remove the incomplete file
    std::remove(image_path.c_str());
    curl_easy_cleanup(curl_handle);
    return false;
  }
  // Request was successful, try to get content type
  std::string content_type_str;
  char* ct = nullptr;
  CURLcode info_res =
      curl_easy_getinfo(curl_handle, CURLINFO_CONTENT_TYPE, &ct);
  if (info_res == CURLE_OK && ct) {
    content_type_str = ct;
    // The content_type string might have extra info like ";
    // charset=UTF-8" We might want to strip that, e.g., find the first
    // ';'
    auto image_dash_pos = content_type_str.find("image/");
    if (image_dash_pos != std::string::npos) {
      content_type_str = content_type_str.substr(image_dash_pos);
    }
    size_t semi_colon_pos = content_type_str.find(';');
    if (semi_colon_pos != std::string::npos) {
      content_type_str = content_type_str.substr(0, semi_colon_pos);
    }
    mime_type = content_type_str;
  } else {
    std::cerr << "Warning: Could not get content type. "
              << curl_easy_strerror(info_res) << std::endl;
    // content_type_str will remain empty or you could set a default
  }

  long http_code = 0;
  curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &http_code);
  if (http_code >= 400) {  // CURLOPT_FAILONERROR
                           // 应该已经处理了大部分，但多一层检查无妨
    std::cerr << "Error: HTTP request failed with code " << http_code
              << std::endl;
    std::remove(image_path.c_str());
    curl_easy_cleanup(curl_handle);
    return false;
  }

  // 7. Clean up the CURL easy handle
  curl_easy_cleanup(curl_handle);

  return true;
}

// Helper function to trim leading/trailing whitespace
static std::string trim(const std::string& str) {
  size_t first = str.find_first_not_of(" \t\n\r\f\v");
  if (std::string::npos == first) {
    return str;
  }
  size_t last = str.find_last_not_of(" \t\n\r\f\v");
  return str.substr(first, (last - first + 1));
}

// Callback function to process received headers
// It specifically looks for the Content-Type header
static size_t header_callback(char* buffer, size_t size, size_t nitems,
                              void* userdata) {
  size_t total_size = size * nitems;
  std::string header_line(buffer, total_size);
  std::string* content_type_ptr = static_cast<std::string*>(userdata);

  // Convert header name to lowercase for case-insensitive comparison
  std::string header_name;
  size_t colon_pos = header_line.find(':');
  if (colon_pos != std::string::npos) {
    header_name = header_line.substr(0, colon_pos);
    std::transform(header_name.begin(), header_name.end(), header_name.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (header_name == "content-type") {
      std::string value = header_line.substr(colon_pos + 1);
      // Trim whitespace and potential extra info (like charset)
      value = trim(value);
      // Sometimes content-type includes charset, e.g., "image/jpeg;
      // charset=utf-8"
      // We only want the MIME type part.
      size_t semi_colon_pos = value.find(';');
      if (semi_colon_pos != std::string::npos) {
        value = value.substr(0, semi_colon_pos);
        value = trim(value);  // Trim again after potentially removing charset
      }
      *content_type_ptr = value;
    }
  }

  // Must return total_size to indicate all data was processed
  return total_size;
}

/**
 * @brief Gets the MIME type (Content-Type header) of a resource at a URL using
 * libcurl.
 *
 * @param url The URL of the resource (e.g., an image).
 * @return The MIME type string (e.g., "image/jpeg", "image/png") if found,
 *         or an empty string if the Content-Type header is not found or an
 * error occurs.
 */
std::string getMIME(std::string const& url, std::string const& proxy) {
  CURL* curl = nullptr;
  CURLcode res = CURLE_OK;
  std::string content_type;  // This will store the result

  // Initialize CURL easy handle
  curl = curl_easy_init();
  if (!curl) {
    std::cerr << "Error: curl_easy_init() failed" << std::endl;
    return "";  // Return empty on init failure
  }

  // Set the URL
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

  // Perform a HEAD request (headers only, no body)
  curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);

  // Follow redirects
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

  // Set the header callback to capture the Content-Type header
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);

  // Pass the address of our content_type string to the callback
  curl_easy_setopt(curl, CURLOPT_HEADERDATA, &content_type);

  // Set timeouts
  curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 10000L);  // 10s total timeout
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS,
                   3000L);  // 3s connect timeout

  if (!proxy.empty()) {
    curl_easy_setopt(curl, CURLOPT_PROXY, proxy.c_str());
  }

  // Perform the request
  res = curl_easy_perform(curl);

  // Check for errors during the request
  if (res != CURLE_OK) {
    std::cerr << "Error: curl_easy_perform() failed for URL '" << url
              << "': " << curl_easy_strerror(res) << std::endl;
    content_type.clear();  // Ensure empty string on error
  } else {
    // Check HTTP response code (optional but good)
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    if (http_code >= 400) {
      std::cerr << "Warning: HTTP error " << http_code << " for URL '" << url
                << "'" << std::endl;
      // Depending on requirements, you might want to clear content_type
      // here too content_type.clear();
    }
    // If content_type is still empty after a successful request,
    // it means the server didn't send a Content-Type header.
    if (content_type.empty() && http_code < 400) {
      std::cerr << "Warning: No Content-Type header found for URL '" << url
                << "'" << std::endl;
    }
  }

  // Cleanup CURL easy handle
  curl_easy_cleanup(curl);

  return content_type;
}

std::string format_timestamp(
    std::chrono::time_point<std::chrono::system_clock> time,
    const char* format) {
  auto time_t_now = std::chrono::system_clock::to_time_t(time);
#if defined(_WIN32)
  struct tm tm{};
  localtime_s(&tm, &time_t_now);
#else
  auto tm = *std::localtime(&time_t_now);
#endif
  char buf[128];
  auto const ret = std::strftime(buf, std::size(buf), format, &tm);
  if (ret == 0) {
    buf[0] = '\0';
  }
  return std::string(buf);
}
std::string format_timenow(const char* format) {
  return format_timestamp(std::chrono::system_clock::now(), format);
}

std::string app_data_dir(const std::string& app,
                         [[maybe_unused]] const std::string& author) {
#if defined(_WIN32)
  auto userprofile = env::get("USERPROFILE");
  if (userprofile) {
    return userprofile.value() + R"(\AppData\Local\)" +
           (author.empty() ? "Ai\\" : author + "\\") + app;
  }
  auto home_drive = env::get("HOMEDRIVE");
  auto home_path = env::get("HOMEPATH");
  if (home_drive.has_value() && home_path.has_value()) {
    return home_drive.value() + home_path.value() + R"(\AppData\Local\)" +
           (author.empty() ? "Ai\\" : author + "\\") + app;
  }
#elif defined(__APPLE__)
  auto home_dir = env::get("HOME");
  if (home_dir.has_value()) {
    return home_dir.value() + "/Library/Application Support/" + app;
  }
#else
  auto home_dir = env::get("HOME");
  if (home_dir.has_value()) {
    return home_dir.value() + "/.local/share/" + app;
  }
#endif
  return fs::current_path().string();
}

template <typename C, typename CharT>
concept has_emplace_back_range = requires(C c, std::basic_string<CharT> s) {
  c.emplace_back(s.begin(), s.end());
};

template <typename C, typename CharT, typename F>
  requires requires(F f, CharT c) {
    { f(c) } -> std::convertible_to<bool>;
  } && (has_emplace_back_range<C, CharT> ||
        requires(C c) {
          c.insert(c.end(), std::declval<std::basic_string<CharT>>());
        })
void split_to_if(
    C& c, const std::basic_string<CharT>& str, F f,
    std::size_t split_count = (std::numeric_limits<std::size_t>::max)(),
    bool compress_tokens = false) {
  auto begin = str.begin();
  auto delimiter = begin;
  std::size_t count = 0;

  if constexpr (requires { c.reserve(std::declval<std::size_t>()); }) {
    if (split_count < (std::numeric_limits<std::size_t>::max)()) {
      c.reserve(c.size() + split_count + 1);
    }
  }

  while ((count++ < split_count) &&
         (delimiter = std::find_if(begin, str.end(), f)) != str.end()) {
    if constexpr (has_emplace_back_range<C, CharT>) {
      c.emplace_back(begin, delimiter);
    } else {
      c.insert(c.end(), std::basic_string<CharT>{begin, delimiter});
    }
    if (compress_tokens) {
      begin = std::find_if_not(delimiter, str.end(), f);
    } else {
      begin = std::next(delimiter);
    }
  }

  if constexpr (has_emplace_back_range<C, CharT>) {
    c.emplace_back(begin, str.end());
  } else {
    c.insert(c.end(), std::basic_string<CharT>{begin, str.end()});
  }
}

static std::string trim_token(const std::string& token) {
  auto start = token.find_first_not_of(" \t");
  auto end = token.find_last_not_of(" \t");
  if (start == std::string::npos) {
    return "";
  }
  return token.substr(start, end - start + 1);
}

std::vector<std::string> split(const std::string& s, char delim) {
  std::vector<std::string> ret;
  std::vector<std::string> tokens;
  split_to_if(tokens, s, [delim](char c) { return c == delim; });
  ret.reserve(tokens.size());
  for (auto& token : tokens) {
    auto trimmed = trim_token(token);
    if (!trimmed.empty()) {
      ret.push_back(std::move(trimmed));
    }
  }
  return ret;
}
std::vector<std::wstring> split(const std::wstring& s, wchar_t delim) {
  std::vector<std::wstring> ret;
  std::vector<std::wstring> tokens;
  split_to_if(tokens, s, [delim](wchar_t c) { return c == delim; });
  for (auto& token : tokens) {
    // Trim leading whitespace
    auto start = token.find_first_not_of(L" \t");
    if (start == std::wstring::npos) {
      continue;  // Skip empty/whitespace-only tokens
    }
    auto end = token.find_last_not_of(L" \t");
    ret.push_back(token.substr(start, end - start + 1));
  }
  return ret;
}

/// Truncate a UTF-8 string to at most @p max_length characters (not bytes).
std::string utf8_truncate(std::string const& s, size_t max_length) {
  utfx::utf8_view u8(s);
  auto ret = u8.substr(0, max_length);
  return std::string(ret.data(), ret.data() + ret.byte_size());
}

}  // namespace ai::utils
