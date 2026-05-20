#include <filesystem>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <optional>
#include <sstream>
#include <string>

#include "ai/function.h"

extern std::string expand_tilde(std::string const& path);
extern std::optional<std::string> resolve_path(nlohmann::json const& args);

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
