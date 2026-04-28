#pragma once

#include <filesystem>
#include <stack>
#include <string>

namespace glob {
namespace detail {

enum class PatternType { CHAR, ANY /*?*/, ZERO_OR_MORE /***/ };
struct PatternItem {
  char value = '\0';
  PatternType type = PatternType::CHAR;
};

inline bool equal(char a, char b, bool ignore_case) {
  return a == b ||
         (ignore_case && ((a ^ b) == 32 && (unsigned)((a | 32) - 'a') <= 25));
}

inline bool matchglob(const std::string& pattern, const std::string& name,
                      bool ignore_case = false) {
  std::vector<PatternItem> items;
  auto it = pattern.begin();
  while (it != pattern.end()) {
    if (*it == '?') {
      items.emplace_back('\0', PatternType::ANY);
      it++;
    } else if (*it == '*') {
      items.emplace_back('\0', PatternType::ZERO_OR_MORE);
      it = std::find_if_not(it, pattern.end(), [](char c) { return c == '*'; });
    } else {
      items.emplace_back(*it, PatternType::CHAR);
      it++;
    }
  }

  auto p = items.begin();
  auto n = name.begin();

  std::stack<std::pair<std::vector<PatternItem>::iterator,
                       std::string::const_iterator>,
             std::vector<std::pair<std::vector<PatternItem>::iterator,
                                   std::string::const_iterator>>>
      backtrack;

  while (true) {
    bool matching = true;

    while (p != items.end() && matching) {
      switch (p->type) {
        case PatternType::ZERO_OR_MORE: {
          if (p + 1 != items.end() && (p + 1)->type == PatternType::ANY) {
            if (n != name.end()) {
              backtrack.emplace(p, n);
            }
          } else {
            if (n != name.end() && *n != '\\' && *n != '/') {
              backtrack.emplace(p, n);
            }

            while (n != name.end() &&
                   (p + 1 == items.end() ||
                    !equal(*n, (p + 1)->value, ignore_case)) &&
                   *n != '\\' && *n != '/') {
              n++;
            }

            if (n != name.end() && *n != '\\' && *n != '/') {
              backtrack.emplace(p, n);
            }
          }
          break;
        }

        case PatternType::ANY: {
          if (n != name.end()) {
            n++;
          } else {
            matching = false;
          }
          break;
        }

        default: {
          if (n == name.end()) {
            matching = false;
          } else if (equal(*n, p->value, ignore_case)) {
            n++;
          } else if (*n == '\\' && p->value == '/') {
            n++;
          } else if (*n == '/' && p->value == '\\') {
            n++;
          } else {
            matching = false;
          }
          break;
        }
      }

      p++;
    }

    if (matching && n == name.end()) {
      return true;
    }

    if (backtrack.empty()) {
      return false;
    }

    p = backtrack.top().first;
    n = backtrack.top().second;
    backtrack.pop();

    n++;
  }
}
}  // namespace detail

inline std::vector<std::string> glob(std::string const& pattern,
                                     std::string const& path,
                                     bool recursive = false,
                                     bool ignore_case = false) {
  std::vector<std::string> result;

  std::error_code ec;

  if (recursive) {
    try {
      for (auto& entry : std::filesystem::recursive_directory_iterator(
               path, std::filesystem::directory_options::skip_permission_denied,
               ec)) {
        if (ec) {
          ec.clear();
          continue;
        }
        if (detail::matchglob(pattern, entry.path().filename().string(),
                              ignore_case)) {
          result.push_back(entry.path().string());
        }
      }
    } catch (...) {
    }
  } else {
    try {
      std::filesystem::directory_iterator it(path, ec);
      if (ec) {
        return result;
      }
      for (auto& entry : it) {
        if (detail::matchglob(pattern, entry.path().filename().string(),
                              ignore_case)) {
          result.push_back(entry.path().string());
        }
      }
    } catch (...) {
    }
  }

  return result;
}

inline std::vector<std::string> iglob(std::string const& pattern,
                                      std::string const& path,
                                      bool recursive = false) {
  return glob(pattern, path, recursive, true);
}

}  // namespace glob
