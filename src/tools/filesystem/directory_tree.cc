#include <filesystem>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

#include "ai/function.h"
#include "tools/filesystem.h"

namespace ai {

namespace {
static nlohmann::json buildTree(std::filesystem::path const& path) {
  std::error_code err;
  if (!std::filesystem::exists(path, err) ||
      !std::filesystem::is_directory(path, err) || err) {
    return nlohmann::json{};
  }
  if (std::filesystem::is_directory(path) &&
      !std::filesystem::is_symlink(path, err)) {
    nlohmann::json ret = nlohmann::json::array();
    for (auto const& entry : std::filesystem::directory_iterator(path, err)) {
      nlohmann::json obj;
      obj["name"] = entry.path().filename();
      if (entry.is_directory() && !entry.is_symlink(err)) {
        obj["type"] = "directory";
        obj["children"] = buildTree(entry.path());
      } else {
        obj["type"] = "file";
      }
      ret.push_back(obj);
    }
    return ret;
  }
  nlohmann::json obj{};
  obj["name"] = path.filename();
  obj["type"] = "file";
  return obj;
}

std::string directory_tree(nlohmann::json const& args) {
  if (!args.is_object()) {
    return "function directory_tree arguments is invalid: expected a JSON "
           "object.";
  }
  auto path_opt = resolve_path(args);
  if (!path_opt.has_value()) {
    return "function directory_tree arguments is invalid: missing required "
           "parameter \"path\".";
  }
  if (path_opt->empty()) {
    return "function directory_tree arguments is invalid: \"path\" must be a "
           "string.";
  }
  std::string path = std::move(*path_opt);
  path = expand_tilde(path);
  print_toolcall_log("directory_tree", {{"path", path}});
  std::error_code err;
  if (!std::filesystem::exists(path, err) ||
      !std::filesystem::is_directory(path, err) || err) {
    return "Error: " + path + " not a directory or not exists";
  }
  return buildTree(path).dump(2);
}
}  // namespace

class DirectoryTreeFunction : public ai::Function {
 public:
  std::string call(nlohmann::json const& args) override {
    return directory_tree(args);
  }
  std::string const& category() const override { return category_; }
  nlohmann::json const& schema() const override { return schema_; }
  [[maybe_unused]] static Function* const registered_;

 private:
  std::string category_ = "filesystem";
  nlohmann::json schema_ = R"===(
{
  "type": "function",
  "name": "directory_tree",
  "description": "Get a recursive tree view of files and directories as a JSON structure. Each entry includes 'name', 'type' (file/directory), and 'children' for directories. Files have no children array, while directories always have a children array (which may be empty). The output is formatted with 2-space indentation for readability.",
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

AUTO_REGISTER(DirectoryTreeFunction);

}  // namespace ai
