#include "temp_file.h"

#ifdef _WIN32
#include <objbase.h>
#endif

#include <filesystem>
#include <string>

#include "base/file.h"
#include "environment/environment.hpp"

namespace ai::base {

namespace fs = std::filesystem;

namespace {
#if defined(_WIN32)
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
std::string create_uuid() {
  GUID guid;

  if (FAILED(CoCreateGuid(&guid))) {
    throw std::runtime_error("CoCreateGuid failed");
  }

  char guid_str[64];

  sprintf_s(guid_str, std::size(guid_str), "%08lX-%04X-%04X-%04X-%012llX",
            guid.Data1, guid.Data2, guid.Data3,
            static_cast<unsigned int>((guid.Data4[0] << 8) | guid.Data4[1]),
            ((unsigned long long)guid.Data4[2] << 40) |
                ((unsigned long long)guid.Data4[3] << 32) |
                ((unsigned long long)guid.Data4[4] << 24) |
                ((unsigned long long)guid.Data4[5] << 16) |
                ((unsigned long long)guid.Data4[6] << 8) |
                ((unsigned long long)guid.Data4[7]));
  return std::string(guid_str);
}
#endif  // _WIN32
}  // namespace

std::string getTempFilePath(std::string const& prefix,
                            std::string const& postfix) {
#ifdef _WIN32
  return to_string(fs::temp_directory_path() /
                   (to_wstring(prefix + create_uuid() + postfix)));
#else
  std::string temp_file_path;

  std::string temp_dir = []() -> std::string {
    auto tmpdir = env::get("TMPDIR");
    if (tmpdir.has_value()) {
      return tmpdir.value();
    }
    return "/tmp";
  }();
  std::string template_str = temp_dir;
  if (!template_str.ends_with('/')) {
    template_str.push_back('/');
  }
  if (!prefix.empty()) {
    template_str += prefix;
  }
  template_str += "XXXXXX";
  // mkstemps is a BSD extension; use standard mkstemp and append suffix
  // after generating the unique path for portability (Linux, *BSD, macOS).
  int fd = mkstemp(template_str.data());
  if (fd == -1) {
    throw std::runtime_error("Failed to create temporary file.");
  }
  close(fd);
  ::remove(template_str.c_str());
  temp_file_path = template_str;
  if (!postfix.empty()) {
    temp_file_path += postfix;
  }
  return temp_file_path;
#endif
}
TempFile::TempFile(std::string const& prefix, std::string const& postfix)
    : path_{getTempFilePath(prefix, postfix)} {}
TempFile::TempFile() : TempFile("", ".tmp") {}
TempFile::~TempFile() {
  if (!path_.empty() && fs::exists(path_)) {
    fs::remove(path_);
  }
}
std::string const& TempFile::path() const { return path_; }
std::optional<std::string> TempFile::content() const {
  return read_file(path_);
}
}  // namespace ai::base
