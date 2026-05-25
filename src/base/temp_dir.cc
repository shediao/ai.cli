#include "temp_dir.h"

#include <filesystem>

#include "environment/environment.hpp"

namespace ai::base {

namespace fs = std::filesystem;
std::string getTempDirPath(std::string const& prefix) {
  std::string temp_dir_path;

#ifdef _WIN32
  char temp_dir[MAX_PATH];
  if (GetTempPathA(MAX_PATH, temp_dir) == 0) {
    throw std::runtime_error("Failed to get temporary directory.");
  }

  // Generate a unique directory name using a temp file name as a base
  char temp_file[MAX_PATH];
  if (GetTempFileNameA(temp_dir, prefix.empty() ? "TMP" : prefix.c_str(), 0,
                       temp_file) == 0) {
    throw std::runtime_error("Failed to create temporary file name.");
  }
  // Remove the temp file and use its name (plus a suffix) as a directory name
  DeleteFileA(temp_file);
  temp_dir_path = std::string(temp_file) + ".d";
  if (!CreateDirectoryA(temp_dir_path.c_str(), nullptr)) {
    throw std::runtime_error("Failed to create temporary directory: " +
                             temp_dir_path);
  }
#else
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
  // mkdtemp creates the directory and replaces XXXXXX with a unique string
  if (mkdtemp(template_str.data()) == nullptr) {
    throw std::runtime_error("Failed to create temporary directory.");
  }
  temp_dir_path = template_str;
#endif
  return temp_dir_path;
}

TempDir::TempDir(std::string const& prefix) : path_{getTempDirPath(prefix)} {}
TempDir::TempDir() : TempDir("") {}
TempDir::~TempDir() {
  if (!path_.empty() && fs::exists(path_)) {
    // On Windows, remove_all() throws on read-only files and on symlinks
    // (especially in MSYS environments).  Reset permissions and remove
    // symlinks explicitly so removal can succeed, and use the error_code
    // overload to never throw.
#if defined(_WIN32)
    try {
      for (auto const& entry : fs::recursive_directory_iterator(path_)) {
        std::error_code ec;
        // Remove symlinks without following them (MSYS symlink emulation
        // can cause remove_all to fail on these).
        if (entry.is_symlink(ec) && !ec) {
          fs::remove(entry.path(), ec);
          continue;
        }
        // Reset read-only attribute so remove_all won't get "Access denied".
        auto perms = entry.status().permissions();
        if ((perms & fs::perms::owner_write) == fs::perms::none) {
          fs::permissions(entry.path(), fs::perms::owner_write,
                          fs::perm_options::add, ec);
        }
      }
    } catch (...) {
      // If iterating fails (e.g. permission denied on a subdirectory),
      // fall through and try remove_all anyway.
    }
#endif
    std::error_code ec;
    fs::remove_all(path_, ec);
  }
}
std::string const& TempDir::path() const { return path_; }

}  // namespace ai::base
