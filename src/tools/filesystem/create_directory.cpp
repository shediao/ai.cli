#include <filesystem>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

#include "ai/function.h"

extern std::string expand_tilde(std::string const& path);
extern std::optional<std::string> resolve_path(nlohmann::json const& args);

std::string create_directory(nlohmann::json const& args) {
  if (!args.is_object()) {
    return "function create_directory arguments is invalid: expected a JSON "
           "object.";
  }
  auto path_opt = resolve_path(args);
  if (!path_opt.has_value()) {
    return "function create_directory arguments is invalid: missing required "
           "parameter \"path\".";
  }
  if (path_opt->empty()) {
    return "function create_directory arguments is invalid: \"path\" must be "
           "a string.";
  }
  std::string path = std::move(*path_opt);
  path = expand_tilde(path);
  print_toolcall_log("create_directory", {{"path", path}});
  std::error_code err;
  std::filesystem::create_directories(path, err);
  if (err) {
    return "Error: " + err.message();
  } else {
    return "Successfully created directory " + path;
  }
}
