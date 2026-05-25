#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>

#include "ai/function.h"
#include "ai/utils.h"
#include "base/file.h"

extern std::string expand_tilde(std::string const& path);
extern std::optional<std::string> resolve_path(nlohmann::json const& args);
extern std::string append_prefix_per_line(std::string_view str,
                                          std::string_view prefix);

std::string write_file(nlohmann::json const& args) {
  if (!args.is_object()) {
    return "function write_file arguments is invalid: expected a JSON "
           "object.";
  }
  auto path_opt = resolve_path(args);
  if (!path_opt.has_value()) {
    return "function write_file arguments is invalid: missing required "
           "parameter \"path\".";
  }
  if (path_opt->empty()) {
    return "function write_file arguments is invalid: \"path\" must be a "
           "string.";
  }
  std::string path = std::move(*path_opt);
  if (!args.contains("content")) {
    return "function write_file arguments is invalid: missing required "
           "parameter \"content\".";
  }
  if (!args["content"].is_string()) {
    return "function write_file arguments is invalid: \"content\" must be a "
           "string.";
  }
  path = expand_tilde(path);
  std::string content = args["content"].get<std::string>();

  print_toolcall_log(
      "write_file",
      {{"path", path}, {"content", append_prefix_per_line(content, "> ")}});

  if (!ai::base::write_file(path, content)) {
    return "Failed to write to " + path;
  }
  return "Successfully wrote to " + path;
}
