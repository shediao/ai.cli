#include <chrono>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

#include "ai/function.h"
#include "tools/filesystem.h"

namespace ai {

namespace {
std::string get_file_info(nlohmann::json const& args) {
  if (!args.is_object()) {
    return "function get_file_info arguments is invalid: expected a JSON "
           "object.";
  }
  auto path_opt = resolve_path(args);
  if (!path_opt.has_value()) {
    return "function get_file_info arguments is invalid: missing required "
           "parameter \"path\".";
  }
  if (path_opt->empty()) {
    return "function get_file_info arguments is invalid: \"path\" must be a "
           "string.";
  }
  std::string path = std::move(*path_opt);
  path = expand_tilde(path);
  print_toolcall_log("get_file_info", {{"path", path}});
  std::error_code err;
  if (!std::filesystem::exists(path, err)) {
    return "Error: " + path + " does not exist.";
  }

  nlohmann::json info;
  info["path"] = path;

  // Type
  if (std::filesystem::is_regular_file(path, err)) {
    info["type"] = "file";
    info["size"] = std::filesystem::file_size(path, err);
  } else if (std::filesystem::is_directory(path, err)) {
    info["type"] = "directory";
  } else if (std::filesystem::is_symlink(path, err)) {
    info["type"] = "symlink";
    info["target"] = std::filesystem::read_symlink(path, err).string();
  } else if (std::filesystem::is_block_file(path, err)) {
    info["type"] = "block";
  } else if (std::filesystem::is_character_file(path, err)) {
    info["type"] = "character";
  } else if (std::filesystem::is_socket(path, err)) {
    info["type"] = "socket";
  } else {
    info["type"] = "other";
  }

  // Last modified time (portable conversion to system_clock)
  auto ftime = std::filesystem::last_write_time(path, err);
  auto sys_time =
      std::chrono::time_point_cast<std::chrono::system_clock::duration>(
          ftime - decltype(ftime)::clock::now() +
          std::chrono::system_clock::now());
  std::time_t last_modified = std::chrono::system_clock::to_time_t(sys_time);
#if defined(_WIN32)
  char buf[64]{0};
  ctime_s(buf, std::size(buf), &last_modified);
  buf[std::size(buf) - 1] = '\0';
  info["last_modified"] = buf;
#else
  info["last_modified"] = std::ctime(&last_modified);
#endif
  // Remove trailing newline from ctime
  if (info["last_modified"].is_string()) {
    std::string ts = info["last_modified"];
    if (!ts.empty() && ts.back() == '\n') {
      ts.pop_back();
    }
    info["last_modified"] = ts;
  }

  // Permissions
  auto perms = std::filesystem::status(path, err).permissions();
  auto to_perm_str = [](std::filesystem::perms p, char r, char w, char x) {
    std::string s;
    s +=
        (p & std::filesystem::perms::owner_read) != std::filesystem::perms::none
            ? r
            : '-';
    s += (p & std::filesystem::perms::owner_write) !=
                 std::filesystem::perms::none
             ? w
             : '-';
    s +=
        (p & std::filesystem::perms::owner_exec) != std::filesystem::perms::none
            ? x
            : '-';
    return s;
  };
  std::string perm_str;
  perm_str += to_perm_str(perms, 'r', 'w', 'x');
  info["permissions"] = perm_str;

  return info.dump();
}
}  // namespace

class GetFileInfoFunction : public ai::Function {
 public:
  std::string call(nlohmann::json const& args) override {
    return get_file_info(args);
  }
  std::string const& category() const override { return category_; }
  nlohmann::json const& schema() const override { return schema_; }

 private:
  std::string category_ = "filesystem";
  nlohmann::json schema_ = R"===(
{
  "type": "function",
  "name": "get_file_info",
  "description": "Retrieve detailed metadata about a file or directory. Returns comprehensive information including size, creation time, last modified time, permissions, and type. This tool is perfect for understanding file characteristics without reading the actual content.",
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

AUTO_REGISTER(GetFileInfoFunction);

}  // namespace ai
