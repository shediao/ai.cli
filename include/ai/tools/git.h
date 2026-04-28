#pragma once

#include <nlohmann/json.hpp>
#include <string_view>

const std::string_view get_git_tools();
void regist_git_tools();
