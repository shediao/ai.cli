#include "tools/filesystem.h"

#include <environment/environment.hpp>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>

namespace ai {

std::string expand_tilde(std::string const& path) {
  if (!path.empty() &&
      ((path.length() == 1 && path[0] == '~') ||
       (path.length() > 1 && path[0] == '~' && path[1] == '/'))) {
    if (auto home = env::get("HOME"); home.has_value()) {
      return home.value() + path.substr(1);
    }
  }
  return path;
}

// Resolve path from args, falling back to "file" if "path" is absent.
// Returns std::nullopt if neither key exists;
// returns an empty string if a key exists but its value is not a string.
std::optional<std::string> resolve_path(nlohmann::json const& args) {
  if (args.contains("path")) {
    if (args["path"].is_string()) {
      return args["path"].get<std::string>();
    }
    return std::string{};
  }
  if (args.contains("file")) {
    if (args["file"].is_string()) {
      return args["file"].get<std::string>();
    }
    return std::string{};
  }
  return std::nullopt;
}

std::string append_prefix_per_line(std::string_view str,
                                   std::string_view prefix) {
  std::string result;
  result.reserve(str.size() + (str.size() * prefix.size()) / 32);
  bool line_start = true;
  for (auto c : str) {
    if (line_start) {
      result.insert(result.end(), prefix.begin(), prefix.end());
      line_start = false;
    }
    result += c;
    if (c == '\n') {
      line_start = true;
    }
  }
  return result;
}

}  // namespace ai
