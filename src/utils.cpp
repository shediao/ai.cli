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

}  // namespace ai::utils
