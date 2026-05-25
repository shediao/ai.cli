#pragma once
#include <optional>
#include <string>

namespace ai::base {
std::optional<std::string> read_file(std::string const& path);
bool write_file(std::string const& path, std::string const& content);
}  // namespace ai::base
