#include <filesystem>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

#include "ai/function.h"
#include "base/glob.h"

namespace ai {

extern std::string expand_tilde(std::string const& path);
extern std::optional<std::string> resolve_path(nlohmann::json const& args);

namespace {
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
  auto matches = ai::base::glob(pattern, path, recursive, ignore_case);
  if (matches.empty() && pattern.find("*") == std::string::npos) {
    pattern = "*" + pattern + "*";
    matches = ai::base::glob(pattern, path, recursive, ignore_case);
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
}  // namespace

class FindFilesFunction : public ai::Function {
 public:
  std::string call(nlohmann::json const& args) override {
    return find_files(args);
  }
  std::string const& category() const override { return category_; }
  nlohmann::json const& schema() const override { return schema_; }

 private:
  std::string category_ = "filesystem";
  nlohmann::json schema_ = R"===(
{
  "type": "function",
  "name": "find_files",
  "description": "Recursively search for files and directories matching a pattern. Searches through all subdirectories from the starting path when recursive is true. The search is case-insensitive by default. If no results are found and the pattern does not contain a wildcard (*), the search automatically retries by wrapping the pattern with wildcards (e.g., \"file\" becomes \"*file*\") for broader matching. Results are returned with [FILE] or [DIR] prefixes and full paths. Great for finding files when you don't know their exact location or name.",
  "parameters": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string",
        "description": "The directory path to search within. Can be an absolute or relative path. Tilde (~) expansion is supported for home directory paths."
      },
      "pattern": {
        "type": "string",
        "description": "The glob pattern to match against file and directory names. Supports standard wildcards (* matches any characters, ? matches a single character). If no wildcard is provided and no results are found, the search automatically falls back to a substring match (wrapping the pattern with * on both sides). Matching is case-insensitive."
      },
      "recursive": {
        "type": "boolean",
        "description": "Whether to search recursively into subdirectories. Defaults to false (only search the immediate directory). Set to true to search through all nested subdirectories."
      }
    },
    "required": ["path", "pattern"]
  }
}
)==="_json;
};

AUTO_REGISTER(FindFilesFunction);

}  // namespace ai
