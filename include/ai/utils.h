#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>  // For HANDLE
#endif

namespace ai::utils {

#if defined(_WIN32)
using NativeHandle = HANDLE;
const NativeHandle INVALID_NATIVE_HANDLE_VALUE = INVALID_HANDLE_VALUE;
#else   // _WIN32
using NativeHandle = int;
constexpr NativeHandle INVALID_NATIVE_HANDLE_VALUE = -1;
#endif  // !_WIN32

template <typename...>
using void_t = void;

template <typename T, typename = void_t<>>
struct is_callable : public std::false_type {};

template <typename F>
struct is_callable<F, decltype(std::declval<F>()())> : public std::true_type {};

template <typename T>
constexpr bool is_callable_v = is_callable<T>::value;

class TempFile {
 public:
  TempFile();
  TempFile(std::string const& prefix, std::string const& postfix);
  ~TempFile();
  const std::string& path() const;
  std::optional<std::string> content() const;

 private:
  std::string path_;
};

class TempDir {
 public:
  TempDir();
  explicit TempDir(std::string const& prefix);
  ~TempDir();
  const std::string& path() const;

 private:
  std::string path_;
};

class Terminal {
 public:
  Terminal();
  ~Terminal();
  Terminal(Terminal const&) = delete;
  Terminal(Terminal&&) = delete;
  Terminal& operator=(Terminal const&) = delete;
  Terminal& operator=(Terminal&&) = delete;

  bool available() const;
  void write(const std::string_view s);

  std::string read_line();
  char read_char();

  bool confirm(std::string_view message, bool default_yes = false);

  std::size_t menu(std::string_view title,
                   std::vector<std::string> const& items);

  static std::string edit(std::string_view initial_content = "");

 private:
  NativeHandle in_{INVALID_NATIVE_HANDLE_VALUE};
  NativeHandle out_{INVALID_NATIVE_HANDLE_VALUE};
};

std::string getTempFilePath(std::string const& prefix,
                            std::string const& postfix);
std::string getTempDirPath(std::string const& prefix);
bool download_image(std::string const& image_url, std::string const& image_path,
                    std::string& mime_type, std::string const& proxy);
std::string getMIME(std::string const& url, std::string const& proxy);

std::string app_data_dir(const std::string& app,
                         const std::string& author = "");

std::string format_timestamp(
    std::chrono::time_point<std::chrono::system_clock> =
        std::chrono::system_clock::now(),
    const char* = "%Y/%m/%d %H:%M:%S %z");
std::string format_timenow(const char* = "%Y/%m/%d %H:%M:%S %z");

// Fast file I/O: single-shot binary read/write with preallocated buffer.
std::optional<std::string> read_file(std::string const& path);
bool write_file(std::string const& path, std::string const& content);

#if defined(_WIN32)
std::optional<std::string> toUtf8(const std::string& s);
#endif

bool stdin_is_atty();
bool stdout_is_atty();
bool stderr_is_atty();

std::vector<std::string> split(const std::string& s, char delim);
std::vector<std::wstring> split(const std::wstring& s, wchar_t delim);

std::string utf8_truncate(std::string const& s, size_t max_length);
}  // namespace ai::utils
