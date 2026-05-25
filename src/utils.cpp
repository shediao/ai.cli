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

/**
 * @brief Gets the MIME type (Content-Type header) of a resource at a URL using
 * libcurl.
 *
 * @param url The URL of the resource (e.g., an image).
 * @return The MIME type string (e.g., "image/jpeg", "image/png") if found,
 *         or an empty string if the Content-Type header is not found or an
 * error occurs.
 */

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
