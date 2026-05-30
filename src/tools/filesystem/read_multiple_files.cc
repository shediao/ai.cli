#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <vector>

#include "ai/function.h"
#include "base/file.h"
#include "tools/filesystem.h"

namespace ai {

namespace {
std::string read_multiple_files(nlohmann::json const& args) {
  if (!args.is_object()) {
    return "function read_multiple_files arguments is invalid: expected a "
           "JSON object.";
  }
  if (!args.contains("paths")) {
    return "function read_multiple_files arguments is invalid: missing "
           "required parameter \"paths\".";
  }
  if (!args["paths"].is_array()) {
    return "function read_multiple_files arguments is invalid: \"paths\" "
           "must be an array.";
  }
  std::vector<std::string> paths;
  std::string paths_str;
  for (auto const& p : args["paths"]) {
    if (p.is_string()) {
      std::string path = p.get<std::string>();
      if (!paths_str.empty()) {
        paths_str += ", ";
      }
      paths_str += path;
      paths.push_back(std::move(path));
    }
  }
  print_toolcall_log("read_multiple_files", {{"paths", paths_str}});
  std::string contents;
  for (auto const& path : paths) {
    std::string expanded_path = expand_tilde(path);
    if (!contents.empty()) {
      contents += "\n------\n";
    }
    auto file_content_opt = ai::base::read_file(expanded_path);
    if (file_content_opt.has_value()) {
      std::string file_content = std::move(file_content_opt.value());
      contents += expanded_path;
      contents += "\n";
      if (!file_content.empty()) {
        contents += file_content;
      } else {
        contents += "(empty)";
      }
    } else {
      contents += expanded_path + " (failed to read)";
    }
  }
  return contents;
}
}  // namespace

class ReadMultipleFilesFunction : public ai::Function {
 public:
  std::string call(nlohmann::json const& args) override {
    return read_multiple_files(args);
  }
  std::string const& category() const override { return category_; }
  nlohmann::json const& schema() const override { return schema_; }

 private:
  std::string category_ = "filesystem";
  nlohmann::json schema_ = R"===(
{
  "type": "function",
  "name": "read_multiple_files",
  "description": "Read the contents of multiple files simultaneously. This is more efficient than reading files one by one when you need to analyze or compare multiple files. Each file's content is returned with its path as a reference. Failed reads for individual files won't stop the entire operation.",
  "parameters": {
    "type": "object",
    "properties": {
      "paths": {
        "type": "array",
        "items": {
          "type": "string"
        }
      }
    },
    "required": ["paths"]
  }
}
)==="_json;
};

AUTO_REGISTER(ReadMultipleFilesFunction);

}  // namespace ai
