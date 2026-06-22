#include <filesystem>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <optional>
#include <sstream>
#include <string>

#include "ai/function.h"
#include "tools/filesystem.h"

namespace ai {

namespace {
std::string disk_space_info(nlohmann::json const& args) {
  if (!args.is_object()) {
    return "function disk_space_info arguments is invalid: expected a JSON "
           "object.";
  }
  auto path_opt = resolve_path(args);
  if (!path_opt.has_value()) {
    return "function disk_space_info arguments is invalid: missing required "
           "parameter \"path\".";
  }
  if (path_opt->empty()) {
    return "function disk_space_info arguments is invalid: \"path\" must be "
           "a string.";
  }
  std::string path = std::move(*path_opt);
  path = expand_tilde(path);
  print_toolcall_log("disk_space_info", {{"path", path}});
  std::error_code err;
  auto space = std::filesystem::space(path, err);
  if (err) {
    return "Error: " + err.message();
  }

  auto format_size = [](std::uintmax_t bytes) -> std::string {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit_idx = 0;
    double size = static_cast<double>(bytes);
    while (size >= 1024.0 && unit_idx < 4) {
      size /= 1024.0;
      ++unit_idx;
    }
    std::ostringstream oss;
    oss.precision(2);
    oss << std::fixed << size << " " << units[unit_idx];
    return oss.str();
  };

  nlohmann::json info;
  info["path"] = path;
  info["capacity"] = space.capacity;
  info["free"] = space.free;
  info["available"] = space.available;
  info["used"] = space.capacity - space.free;
  info["capacity_human"] = format_size(space.capacity);
  info["free_human"] = format_size(space.free);
  info["available_human"] = format_size(space.available);
  info["used_human"] = format_size(space.capacity - space.free);

  if (space.capacity > 0) {
    double pct = 100.0 * (space.capacity - space.free) /
                 static_cast<double>(space.capacity);
    std::ostringstream pct_oss;
    pct_oss.precision(2);
    pct_oss << std::fixed << pct << "%";
    info["used_percent"] = pct_oss.str();
  }

  return info.dump();
}
}  // namespace

class DiskSpaceInfoFunction : public ai::Function {
 public:
  std::string call(nlohmann::json const& args) override {
    return disk_space_info(args);
  }
  std::string const& category() const override { return category_; }
  nlohmann::json const& schema() const override { return schema_; }
  [[maybe_unused]] static Function* const registered_;

 private:
  std::string category_ = "filesystem";
  nlohmann::json schema_ = R"===(
{
  "type": "function",
  "name": "disk_space_info",
  "description": "Get disk space information for the filesystem containing the specified path. Returns total capacity, free space, available space, used space, and usage percentage. Use this tool to check disk space availability before writing large files or when troubleshooting storage issues.",
  "parameters": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string",
        "description": "Any path on the filesystem to query. The disk space for the filesystem containing this path will be returned."
      }
    },
    "required": ["path"]
  }
}
)==="_json;
};

AUTO_REGISTER(DiskSpaceInfoFunction);

}  // namespace ai
