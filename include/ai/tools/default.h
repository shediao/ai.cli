#pragma once

#include <nlohmann/json.hpp>
#include <string_view>

const std::string_view get_default_tools();
void regist_default_tools();
