#include "ai/glob.h"

#include "ai/matchglob.h"

namespace fs = std::filesystem;
namespace ai {
std::vector<std::filesystem::path> glob(std::string_view pattern, fs::path dir,
                                        bool recursive, bool ignore_case) {
  std::vector<fs::path> result;

  std::error_code ec;

  auto match_entry = [&](const fs::directory_entry& entry) {
    if (ai::matchglob(pattern, entry.path().filename().string(), ignore_case)) {
      result.push_back(entry.path());
    }
  };

  if (recursive) {
    try {
      for (auto& entry : fs::recursive_directory_iterator(
               dir, fs::directory_options::skip_permission_denied, ec)) {
        if (ec) {
          ec.clear();
          continue;
        }
        match_entry(entry);
      }
    } catch (...) {
    }
  } else {
    try {
      fs::directory_iterator it(dir, ec);
      if (ec) {
        return result;
      }
      for (auto& entry : it) {
        match_entry(entry);
      }
    } catch (...) {
    }
  }

  return result;
}

}  // namespace ai
