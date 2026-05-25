#include "matchglob.h"

#include <algorithm>
#include <stack>
#include <vector>

namespace ai::base {
namespace {
enum class PatternType { CHAR, ANY /*?*/, ZERO_OR_MORE /***/ };

template <typename CharT>
struct PatternItem {
  CharT value = 0;
  PatternType type = PatternType::CHAR;
};

template <typename CharT>
inline bool equal(CharT a, CharT b, bool ignore_case) {
  return a == b ||
         (ignore_case && ((a ^ b) == 32 && (unsigned)((a | 32) - 'a') <= 25));
}

template <typename CharT>
static bool matchglob_impl(std::basic_string_view<CharT> pattern,
                           std::basic_string_view<CharT> name,
                           bool ignore_case) {
  std::vector<PatternItem<CharT>> items;
  auto it = pattern.begin();
  while (it != pattern.end()) {
    if (*it == '?') {
      items.emplace_back('\0', PatternType::ANY);
      it++;
    } else if (*it == '*') {
      items.emplace_back('\0', PatternType::ZERO_OR_MORE);
      it = std::find_if_not(it, pattern.end(), [](auto c) { return c == '*'; });
    } else {
      items.emplace_back(*it, PatternType::CHAR);
      it++;
    }
  }

  auto p = items.begin();
  auto n = name.begin();

  std::stack<std::pair<typename std::vector<PatternItem<CharT>>::iterator,
                       typename std::basic_string_view<CharT>::const_iterator>,
             std::vector<std::pair<
                 typename std::vector<PatternItem<CharT>>::iterator,
                 typename std::basic_string_view<CharT>::const_iterator>>>
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
}  // namespace

bool matchglob(std::string_view pattern, std::string_view string,
               bool ignore_case) {
  return matchglob_impl(pattern, string, ignore_case);
}
bool matchglob(std::wstring_view pattern, std::wstring_view string,
               bool ignore_case) {
  return matchglob_impl(pattern, string, ignore_case);
}
}  // namespace ai::base
