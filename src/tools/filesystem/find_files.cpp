#include <filesystem>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

#include "ai/function.h"
#include "ai/glob.h"

extern std::string expand_tilde(std::string const& path);
extern std::optional<std::string> resolve_path(nlohmann::json const& args);

std::string find_files(nlohmann::json const& args) {
  if (!args.is_object()) {
    return "function find_files arguments is invalid: expected a JSON "
           "object.";
  }
  auto path_opt = resolve_path(args);
  if (!path_opt.has_value()) {
    return "function find_files arguments is invalid: missing required "
           "parameter \"path\".";
  }
  if (path_opt->empty()) {
    return "function find_files arguments is invalid: \"path\" must be a "
           "string.";
  }
  std::string path = std::move(*path_opt);
  if (!args.contains("pattern")) {
    return "function find_files arguments is invalid: missing required "
           "parameter \"pattern\".";
  }
  if (!args["pattern"].is_string()) {
    return "function find_files arguments is invalid: \"pattern\" must be "
           "a string.";
  }
  path = expand_tilde(path);
  std::string pattern = args["pattern"].get<std::string>();
  bool recursive = false;
  if (args.contains("recursive") && args["recursive"].is_boolean()) {
    recursive = args["recursive"].get<bool>();
  }

  print_toolcall_log("find_files",
                     {{"path", path},
                      {"pattern", pattern},
                      {"recursive", recursive ? "true" : "false"}});

  bool ignore_case = true;
  auto matches = ai::glob(pattern, path, recursive, ignore_case);
  if (matches.empty() && pattern.find("*") == std::string::npos) {
    pattern = "*" + pattern + "*";
    matches = ai::glob(pattern, path, recursive, ignore_case);
  }

  std::string ret;
  for (auto const& entry : matches) {
    if (std::filesystem::is_directory(entry)) {
      ret += "[DIR] " + entry.string() + "\n";
    } else {
      ret += "[FILE] " + entry.string() + "\n";
    }
  }

  if (ret.empty()) {
    return "No files or directories matching \"" + pattern + "\" found in " +
           path;
  }
  return ret;
}
