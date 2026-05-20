#include <filesystem>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

#include "ai/function.h"

extern std::string expand_tilde(std::string const& path);
extern std::optional<std::string> resolve_path(nlohmann::json const& args);

std::string list_directory(nlohmann::json const& args) {
  if (!args.is_object()) {
    return "function list_directory arguments is invalid: expected a JSON "
           "object.";
  }
  auto path_opt = resolve_path(args);
  if (!path_opt.has_value()) {
    return "function list_directory arguments is invalid: missing required "
           "parameter \"path\".";
  }
  if (path_opt->empty()) {
    return "function list_directory arguments is invalid: \"path\" must be a "
           "string.";
  }
  std::string path = std::move(*path_opt);
  path = expand_tilde(path);
  print_toolcall_log("list_directory", {{"path", path}});
  std::error_code err;
  if (!std::filesystem::exists(path, err) ||
      !std::filesystem::is_directory(path, err) || err) {
    return "Error: " + err.message();
  }
  std::string ret;
  for (auto const& entry : std::filesystem::directory_iterator(path, err)) {
    if (entry.is_directory()) {
      ret += "[DIR] " + entry.path().string() + "\n";
    } else {
      ret += "[FILE] " + entry.path().string() + "\n";
    }
  }
  return ret;
}
