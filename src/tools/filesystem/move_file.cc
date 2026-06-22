#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>

#include "ai/function.h"
#include "tools/filesystem.h"

namespace ai {

namespace {
std::string move_file(nlohmann::json const& args) {
  if (!args.is_object()) {
    return "function move_file arguments is invalid: expected a JSON object.";
  }
  if (!args.contains("source")) {
    return "function move_file arguments is invalid: missing required "
           "parameter \"source\".";
  }
  if (!args["source"].is_string()) {
    return "function move_file arguments is invalid: \"source\" must be a "
           "string.";
  }
  if (!args.contains("distination")) {
    return "function move_file arguments is invalid: missing required "
           "parameter \"distination\".";
  }
  if (!args["distination"].is_string()) {
    return "function move_file arguments is invalid: \"distination\" must be "
           "a string.";
  }
  std::string source = args["source"].get<std::string>();
  source = expand_tilde(source);
  std::string distination = args["distination"].get<std::string>();
  distination = expand_tilde(distination);
  print_toolcall_log("move_file",
                     {{"source", source}, {"distination", distination}});
  std::error_code err;
  std::filesystem::rename(source, distination, err);

  if (err) {
    return "Error: " + err.message();
  }
  return "Successfully moved " + source + " to " + distination;
}
}  // namespace

class MoveFileFunction : public ai::Function {
 public:
  std::string call(nlohmann::json const& args) override {
    return move_file(args);
  }
  std::string const& category() const override { return category_; }
  nlohmann::json const& schema() const override { return schema_; }
  [[maybe_unused]] static Function* const registered_;

 private:
  std::string category_ = "filesystem";
  nlohmann::json schema_ = R"===(
{
  "type": "function",
  "name": "move_file",
  "description": "Move or rename files and directories. Can move files between directories and rename them in a single operation. If the destination exists, the operation will fail. Works across different directories and can be used for simple renaming within the same directory. Both source and destination must be within allowed directories.",
  "parameters": {
    "type": "object",
    "properties": {
      "source": {
        "type": "string"
      },
      "distination": {
        "type": "string"
      }
    },
    "required": ["source", "distination"]
  }
}
)==="_json;
};

AUTO_REGISTER(MoveFileFunction);

}  // namespace ai
