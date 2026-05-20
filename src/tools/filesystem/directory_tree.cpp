#include <filesystem>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

#include "ai/function.h"

extern std::string expand_tilde(std::string const& path);
extern std::optional<std::string> resolve_path(nlohmann::json const& args);

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
  } else {
    nlohmann::json obj{};
    obj["name"] = path.filename();
    obj["type"] = "file";
    return obj;
  }
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
