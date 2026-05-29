#include <filesystem>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

#include "ai/function.h"

namespace ai {

extern std::string expand_tilde(std::string const& path);
extern std::optional<std::string> resolve_path(nlohmann::json const& args);

namespace {
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
}  // namespace

class ListDirectoryFunction : public ai::Function {
 public:
  std::string call(nlohmann::json const& args) override {
    return list_directory(args);
  }
  std::string const& category() const override { return category_; }
  nlohmann::json const& schema() const override { return schema_; }

 private:
  std::string category_ = "filesystem";
  nlohmann::json schema_ = R"===(
{
  "type": "function",
  "name": "list_directory",
  "description": "Get a detailed listing of all files and directories in a specified path. Results clearly distinguish between files and directories with [FILE] and [DIR] prefixes. This tool is essential for understanding directory structure and finding specific files within a directory.",
  "parameters": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string"
      }
    },
    "required": ["path"]
  }
}
)==="_json;
};

AUTO_REGISTER(ListDirectoryFunction);

}  // namespace ai
