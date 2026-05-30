#pragma once

#include <nlohmann/json_fwd.hpp>
#include <optional>
#include <string>
#include <string_view>

namespace ai {

std::string expand_tilde(std::string const& path);

std::optional<std::string> resolve_path(nlohmann::json const& args);

std::string append_prefix_per_line(std::string_view str,
                                   std::string_view prefix);

}  // namespace ai
