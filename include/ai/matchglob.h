#pragma once

#include <string_view>

namespace ai {
bool matchglob(std::string_view pattern, std::string_view string,
               bool ignore_case);
bool matchglob(std::wstring_view pattern, std::wstring_view string,
               bool ignore_case);
}  // namespace ai
