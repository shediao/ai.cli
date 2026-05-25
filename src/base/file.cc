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
  std::ifstream file(path);
  if (!file.is_open()) {
    return std::nullopt;
  }
  return std::string{std::istreambuf_iterator<char>(file),
                     std::istreambuf_iterator<char>()};
}

bool write_file(std::string const& path, std::string const& content) {
  std::ofstream file(path);
  if (!file.is_open()) {
    return false;
  }
  file.write(content.data(), static_cast<std::streamsize>(content.size()));
  file.flush();
  return file.good();
}
}  // namespace ai::base
