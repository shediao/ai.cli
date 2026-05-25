#include "download.h"

#include <curl/curl.h>

#include <algorithm>
#include <fstream>

#include "base/logging.h"

namespace ai::base {

namespace {
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

}  // namespace
bool download(std::string const& image_url, std::string const& image_path,
              std::string& mime_type, std::string const& proxy) {
  CURL* curl_handle;
  CURLcode res;
  std::ofstream outfile;

  // 1. Obtain a CURL easy handle
  curl_handle = curl_easy_init();
  if (!curl_handle) {
    LOG(ERROR) << "Error: curl_easy_init() failed." << std::endl;
    return false;
  }

  // 2. Open local file for writing (binary mode)
  outfile.open(image_path, std::ios::binary);
  if (!outfile.is_open()) {
    LOG(ERROR) << "Error: Cannot open file for writing: " << image_path
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
    LOG(ERROR) << "Error: curl_easy_perform() failed: "
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
    LOG(ERROR) << "Warning: Could not get content type. "
               << curl_easy_strerror(info_res) << std::endl;
    // content_type_str will remain empty or you could set a default
  }

  long http_code = 0;
  curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &http_code);
  if (http_code >= 400) {  // CURLOPT_FAILONERROR
                           // 应该已经处理了大部分，但多一层检查无妨
    LOG(ERROR) << "Error: HTTP request failed with code " << http_code
               << std::endl;
    std::remove(image_path.c_str());
    curl_easy_cleanup(curl_handle);
    return false;
  }

  // 7. Clean up the CURL easy handle
  curl_easy_cleanup(curl_handle);

  return true;
}

std::string getMIME(std::string const& url, std::string const& proxy) {
  CURL* curl = nullptr;
  CURLcode res = CURLE_OK;
  std::string content_type;  // This will store the result

  // Initialize CURL easy handle
  curl = curl_easy_init();
  if (!curl) {
    LOG(ERROR) << "Error: curl_easy_init() failed" << std::endl;
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
    LOG(ERROR) << "Error: curl_easy_perform() failed for URL '" << url
               << "': " << curl_easy_strerror(res) << std::endl;
    content_type.clear();  // Ensure empty string on error
  } else {
    // Check HTTP response code (optional but good)
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    if (http_code >= 400) {
      LOG(ERROR) << "Warning: HTTP error " << http_code << " for URL '" << url
                 << "'" << std::endl;
      // Depending on requirements, you might want to clear content_type
      // here too content_type.clear();
    }
    // If content_type is still empty after a successful request,
    // it means the server didn't send a Content-Type header.
    if (content_type.empty() && http_code < 400) {
      LOG(ERROR) << "Warning: No Content-Type header found for URL '" << url
                 << "'" << std::endl;
    }
  }

  // Cleanup CURL easy handle
  curl_easy_cleanup(curl);

  return content_type;
}

}  // namespace ai::base
