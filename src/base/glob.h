#pragma once
#include <filesystem>
#include <string_view>
#include <vector>

namespace ai::base {
std::vector<std::filesystem::path> glob(std::string_view pattern,
                                        std::filesystem::path dir,
                                        bool recursive = false,
                                        bool ignore_case = false);

}  // namespace ai::base
