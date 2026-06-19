#include "file.h"

#include <filesystem>
#include <fstream>

namespace ai::base {
namespace fs = std::filesystem;

std::optional<std::string> read_file(std::string const& path) {
  // On some platforms (e.g. Linux/GCC) std::ifstream::open() can succeed on a
  // directory, but reading from it later throws an exception.  Check early.
  std::error_code ec;
  if (fs::is_directory(path, ec)) {
    return std::nullopt;
  }
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) {
    return std::nullopt;
  }
  std::string content;
  file.seekg(0, std::ios::end);
  auto size = file.tellg();
  if (size > 0) {
    content.reserve(static_cast<std::string::size_type>(size));
  }
  file.seekg(0, std::ios::beg);
  content.assign(std::istreambuf_iterator<char>(file),
                 std::istreambuf_iterator<char>());
  return content;
}

bool write_file(std::string const& path, std::string const& content) {
  std::ofstream file(path, std::ios::binary);
  if (!file.is_open()) {
    return false;
  }
  file.write(content.data(), static_cast<std::streamsize>(content.size()));
  file.flush();
  return file.good();
}
}  // namespace ai::base
