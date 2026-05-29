#include <filesystem>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

#include "ai/function.h"

namespace ai {

extern std::string expand_tilde(std::string const& path);
extern std::optional<std::string> resolve_path(nlohmann::json const& args);

namespace {
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
  }
  return "Successfully created directory " + path;
}
}  // namespace

class CreateDirectoryFunction : public ai::Function {
 public:
  std::string call(nlohmann::json const& args) override {
    return create_directory(args);
  }
  std::string const& category() const override { return category_; }
  nlohmann::json const& schema() const override { return schema_; }

 private:
  std::string category_ = "filesystem";
  nlohmann::json schema_ = R"===(
{
  "type": "function",
  "name": "create_directory",
  "description": "Create a new directory or ensure a directory exists. Can create multiple nested directories in one operation. If the directory already exists, this operation will succeed silently. Perfect for setting up directory structures for projects or ensuring required paths exist.",
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

AUTO_REGISTER(CreateDirectoryFunction);

}  // namespace ai
