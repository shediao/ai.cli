#pragma once

#include <nlohmann/json_fwd.hpp>
#include <string>
namespace ai {
std::string filter_lines(std::string const& text,
                         nlohmann::json const& filters);

void add_filter_parameter(nlohmann::json& schema);
}  // namespace ai
