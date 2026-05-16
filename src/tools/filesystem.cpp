#include <algorithm>
#include <chrono>
#include <environment/environment.hpp>
#include <filesystem>
#include <iostream>
#include <nlohmann/json.hpp>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <subprocess/subprocess.hpp>

#include "./glob.hpp"
#include "ai/function.h"
#include "ai/utils.h"
#include "filesystem_tools_json.h"

static std::string expand_tilde(std::string const& path) {
  if (!path.empty() &&
      ((path.length() == 1 && path[0] == '~') ||
       (path.length() > 1 && path[0] == '~' && path[1] == '/'))) {
    if (auto home = env::get("HOME"); home.has_value()) {
      return home.value() + path.substr(1);
    }
  }
  return path;
}

// Resolve path from args, falling back to "file" if "path" is absent.
// Returns std::nullopt if neither key exists;
// returns an empty string if a key exists but its value is not a string.
static std::optional<std::string> resolve_path(nlohmann::json const& args) {
  if (args.contains("path")) {
    if (args["path"].is_string()) {
      return args["path"].get<std::string>();
    }
    return std::string{};
  }
  if (args.contains("file")) {
    if (args["file"].is_string()) {
      return args["file"].get<std::string>();
    }
    return std::string{};
  }
  return std::nullopt;
}

static std::string append_prefix_per_line(std::string_view str,
                                          std::string_view prefix) {
  std::string result;
  result.reserve(str.size() + (str.size() * prefix.size()) / 32);
  bool line_start = true;
  for (auto c : str) {
    if (line_start) {
      result.insert(result.end(), prefix.begin(), prefix.end());
      line_start = false;
    }
    result += c;
    if (c == '\n') {
      line_start = true;
    }
  }
  return result;
}

std::string read_file(nlohmann::json const& args) {
  if (!args.is_object()) {
    return "function read_file arguments is invalid: expected a JSON object.";
  }
  auto path_opt = resolve_path(args);
  if (!path_opt.has_value()) {
    return "function read_file arguments is invalid: missing required "
           "parameter \"path\".";
  }
  if (path_opt->empty()) {
    return "function read_file arguments is invalid: \"path\" must be a "
           "string.";
  }
  std::string path = std::move(*path_opt);
  path = expand_tilde(path);
  bool has_offset =
      args.contains("offset") && args["offset"].is_number_integer();
  bool has_limit = args.contains("limit") && args["limit"].is_number_integer();

  std::vector<std::pair<std::string, std::string>> params = {{"path", path}};
  if (has_offset) {
    params.emplace_back("offset", std::to_string(args["offset"].get<int>()));
  }
  if (has_limit) {
    params.emplace_back("limit", std::to_string(args["limit"].get<int>()));
  }
  print_toolcall_log("read_file", params);

  auto content_opt = ai::utils::read_file(path);
  if (!content_opt.has_value()) {
    return path + " is not exists.";
  }
  std::string content = std::move(content_opt.value());
  if (content.empty()) {
    return path + " is empty.";
  }

  if (has_limit || has_offset) {
    int limit = has_limit ? args["limit"].get<int>() : -1;
    int offset = has_offset ? args["offset"].get<int>() : 1;
    if (offset < 1) {
      offset = 1;
    }
    // Count total lines for accurate error reporting
    int total_lines =
        static_cast<int>(std::count(content.begin(), content.end(), '\n'));
    // A non-empty file without trailing newline still has 1 line;
    // a file ending with newline has that many lines.
    if (!content.empty() && content.back() != '\n') {
      ++total_lines;
    }

    auto it = content.begin();
    for (int i = 0; i < offset - 1; ++i) {
      it = std::find(it, content.end(), '\n');
      if (it == content.end()) {
        return path + " has only " + std::to_string(total_lines) +
               " lines, offset " + std::to_string(offset) + " is out of range.";
      }
      ++it;
    }

    if (limit == 0) {
      return std::string{};  // limit=0 means read zero lines
    } else if (limit > 0) {
      auto end = it;
      for (int i = 0; i < limit; ++i) {
        end = std::find(end, content.end(), '\n');
        if (end == content.end()) {
          break;
        }
        ++end;
      }
      return std::string(it, end);
    } else {
      return std::string(it, content.end());
    }
  }

  return content;
}

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
    auto file_content_opt = ai::utils::read_file(expanded_path);
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

  if (!ai::utils::write_file(path, content)) {
    return "Failed to write to " + path;
  }
  return "Successfully wrote to " + path;
}

// Finds the earliest-occurring label from |labels| in |str| starting at
// |start_pos|. Returns {npos, ""} if none match.
std::pair<size_t, std::string_view> find_by_lables(
    const std::string& str, size_t start_pos,
    std::vector<std::string_view> const& lables) {
  size_t best_pos = std::string::npos;
  std::string_view best_label;
  for (auto const& label : lables) {
    size_t pos = str.find(label, start_pos);
    if (pos != std::string::npos && pos < best_pos) {
      best_pos = pos;
      best_label = label;
    }
  }
  return {best_pos, best_label};
}

std::string edit_file(nlohmann::json const& args) {
  // --- argument validation ---
  if (!args.is_object()) {
    return "function edit_file arguments is invalid: expected a JSON object.";
  }
  auto path_opt = resolve_path(args);
  if (!path_opt.has_value()) {
    return "function edit_file arguments is invalid: missing required "
           "parameter \"path\".";
  }
  if (path_opt->empty()) {
    return "function edit_file arguments is invalid: \"path\" must be a "
           "string.";
  }
  if (!args.contains("diff")) {
    return "function edit_file arguments is invalid: missing required "
           "parameter \"diff\".";
  }
  if (!args["diff"].is_string()) {
    return "function edit_file arguments is invalid: \"diff\" must be a "
           "string.";
  }

  std::string path = std::move(*path_opt);
  path = expand_tilde(path);
  std::string diff = args["diff"].get<std::string>();

  print_toolcall_log("edit_file", {{"path", path}, {"diff", diff}});

  // --- read original file ---
  auto file_content_opt = ai::utils::read_file(path);
  if (!file_content_opt.has_value()) {
    return "Failed to open file: " + path;
  }
  std::string file_content = std::move(file_content_opt.value());

#define SEARCH_LABLE "<<<<<<< SEARCH"
#define SEPARATOR_LABLE "======="
#define REPLACE_LABLE ">>>>>>> REPLACE"

  // 1. 确认是否以 '<<<<<<< SEARCH' 开头
  if (!diff.starts_with(SEARCH_LABLE "\n")) {
    return "Failed to edit file " + path +
           ": diff must start with \"<<<<<<< SEARCH\" followed by a "
           "newline.";
  }
  // 2. 是否以 '

  std::vector<std::string::size_type> search_indexes;     // <<<<<<< SEARCH
  std::vector<std::string::size_type> separator_indexes;  // =======
  std::vector<std::string::size_type> replace_indexes;    // >>>>>>> REPLACE

  auto it = diff.find(SEARCH_LABLE "\n", 0);
  while (it != std::string::npos) {
    search_indexes.push_back(it);
    it = diff.find(SEARCH_LABLE "\n", it + 15);
  }

  it = diff.find("\n" SEPARATOR_LABLE "\n", 0);
  while (it != std::string::npos) {
    separator_indexes.push_back(it + 1);
    it = diff.find("\n" SEPARATOR_LABLE "\n", it + 9);
  }

  it = diff.find("\n" REPLACE_LABLE, 0);
  while (it != std::string::npos) {
    replace_indexes.push_back(it + 1);
    it = diff.find("\n" REPLACE_LABLE, it + 16);
  }

  it = diff.find("\n" SEPARATOR_LABLE REPLACE_LABLE, 0);
  while (it != std::string::npos) {
    separator_indexes.push_back(it + 1);
    replace_indexes.push_back(it + 1 + 7);
    it = diff.find("\n" SEPARATOR_LABLE REPLACE_LABLE, it + 23);
  }

  // 3. 三个标签的个数应该一样
  if (search_indexes.size() != separator_indexes.size() ||
      separator_indexes.size() != replace_indexes.size()) {
    return "Failed to edit file " + path +
           ": mismatched number of SEARCH/SEPARATOR/REPLACE labels in "
           "diff. Found " +
           std::to_string(search_indexes.size()) + " SEARCH, " +
           std::to_string(separator_indexes.size()) + " separator, " +
           std::to_string(replace_indexes.size()) + " REPLACE labels.";
  }

  // 4. 检测每个标签组是否匹配
  for (size_t i = 0; i < search_indexes.size(); i++) {
    if (search_indexes[i] >= separator_indexes[i] ||
        separator_indexes[i] >= replace_indexes[i]) {
      return "Failed to edit file " + path +
             ": SEARCH/SEPARATOR/REPLACE labels are out of order in "
             "diff block " +
             std::to_string(i + 1) + ".";
    }
  }

  for (size_t i = 0; i < search_indexes.size(); i++) {
    std::string_view search{diff.data() + search_indexes[i] + 15,
                            separator_indexes[i] - search_indexes[i] - 15};
    std::string_view replace{diff.data() + separator_indexes[i] + 8,
                             replace_indexes[i] - separator_indexes[i] - 8};
    auto search_it = file_content.find(search);
    if (search_it != std::string::npos) {
      file_content.replace(search_it, search.size(), replace);
    } else {
      return "Failed to edit file " + path + ": SEARCH block " +
             std::to_string(i + 1) +
             " was not found in the file. The content to replace may "
             "have already been modified or does not exactly match.";
    }
  }

  // --- show diff between original file and modified content ---
  {
    ai::utils::TempFile temp("",
                             std::filesystem::path(path).filename().string());
    ai::utils::write_file(temp.path(), file_content);
    using namespace subprocess::named_arguments;
    using subprocess::run;
    if (0 == run(std::string("which"), "delta", std_out > devnull,
                 std_err > devnull)) {
      run("delta", "--paging=never", path, temp.path());
    } else {
      run("diff", "-U0", "--color=always", path, temp.path());
    }
  }

  // --- persist modified content back to the original file ---
  if (!ai::utils::write_file(path, file_content)) {
    return "Failed to write to file: " + path;
  }

  return "Successfully edited file " + path;
}

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
  } else {
    return "Successfully created directory " + path;
  }
}

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

nlohmann::json buildTree(std::filesystem::path const& path) {
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

std::string search_files(nlohmann::json const& args) {
  if (!args.is_object()) {
    return "function search_files arguments is invalid: expected a JSON "
           "object.";
  }
  auto path_opt = resolve_path(args);
  if (!path_opt.has_value()) {
    return "function search_files arguments is invalid: missing required "
           "parameter \"path\".";
  }
  if (path_opt->empty()) {
    return "function search_files arguments is invalid: \"path\" must be a "
           "string.";
  }
  std::string path = std::move(*path_opt);
  if (!args.contains("pattern")) {
    return "function search_files arguments is invalid: missing required "
           "parameter \"pattern\".";
  }
  if (!args["pattern"].is_string()) {
    return "function search_files arguments is invalid: \"pattern\" must be "
           "a string.";
  }
  path = expand_tilde(path);
  std::string pattern = args["pattern"].get<std::string>();
  bool recursive = false;
  if (args.contains("recursive") && args["recursive"].is_boolean()) {
    recursive = args["recursive"].get<bool>();
  }

  print_toolcall_log("search_files",
                     {{"path", path},
                      {"pattern", pattern},
                      {"recursive", recursive ? "true" : "false"}});

  bool ignore_case = true;
  auto matches = glob::glob(pattern, path, recursive, ignore_case);
  if (matches.empty() && pattern.find("*") == std::string::npos) {
    pattern = "*" + pattern + "*";
    matches = glob::glob(pattern, path, recursive, ignore_case);
  }

  std::string ret;
  for (auto const& entry : matches) {
    if (std::filesystem::is_directory(entry)) {
      ret += "[DIR] " + entry + "\n";
    } else {
      ret += "[FILE] " + entry + "\n";
    }
  }

  if (ret.empty()) {
    return "No files or directories matching \"" + pattern + "\" found in " +
           path;
  }
  return ret;
}

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
  } else {
    return "Successfully moved " + source + " to " + distination;
  }
}

std::string replace_lines(nlohmann::json const& args) {
  // ── argument validation ──
  if (!args.is_object()) {
    return "function replace_lines arguments is invalid: expected a JSON "
           "object.";
  }
  auto path_opt = resolve_path(args);
  if (!path_opt.has_value()) {
    return "function replace_lines arguments is invalid: missing required "
           "parameter \"path\".";
  }
  if (path_opt->empty()) {
    return "function replace_lines arguments is invalid: \"path\" must be a "
           "string.";
  }
  std::string path = std::move(*path_opt);
  if (!args.contains("start_line")) {
    return "function replace_lines arguments is invalid: missing required "
           "parameter \"start_line\".";
  }
  if (!args["start_line"].is_number_integer()) {
    return "function replace_lines arguments is invalid: \"start_line\" must "
           "be an integer.";
  }
  if (!args.contains("end_line")) {
    return "function replace_lines arguments is invalid: missing required "
           "parameter \"end_line\".";
  }
  if (!args["end_line"].is_number_integer()) {
    return "function replace_lines arguments is invalid: \"end_line\" must "
           "be an integer.";
  }
  if (!args.contains("content")) {
    return "function replace_lines arguments is invalid: missing required "
           "parameter \"content\".";
  }
  if (!args["content"].is_string()) {
    return "function replace_lines arguments is invalid: \"content\" must be "
           "a string.";
  }

  path = expand_tilde(path);
  int start_line = args["start_line"].get<int>();
  int end_line = args["end_line"].get<int>();
  std::string content = args["content"].get<std::string>();

  print_toolcall_log("replace_lines",
                     {{"path", path},
                      {"start_line", std::to_string(start_line)},
                      {"end_line", std::to_string(end_line)},
                      {"content", append_prefix_per_line(content, "> ")}});

  if (start_line < 1) {
    return "function replace_lines: \"start_line\" must be >= 1 (1-indexed), "
           "got " +
           std::to_string(start_line);
  }
  if (end_line < start_line) {
    return "function replace_lines: \"end_line\" (" + std::to_string(end_line) +
           ") must be >= \"start_line\" (" + std::to_string(start_line) + ")";
  }

  // ── read original file ──
  auto file_content_opt = ai::utils::read_file(path);
  if (!file_content_opt.has_value()) {
    return "Failed to open file: " + path;
  }
  std::string file_content = std::move(file_content_opt.value());

  // Count total lines
  int total_lines = static_cast<int>(
      std::count(file_content.begin(), file_content.end(), '\n'));
  if (!file_content.empty() && file_content.back() != '\n') {
    ++total_lines;
  }

  if (start_line > total_lines) {
    return "function replace_lines: \"start_line\" " +
           std::to_string(start_line) + " is out of range. File \"" + path +
           "\" has only " + std::to_string(total_lines) + " lines.";
  }
  if (end_line > total_lines) {
    return "function replace_lines: \"end_line\" " + std::to_string(end_line) +
           " is out of range. File \"" + path + "\" has only " +
           std::to_string(total_lines) + " lines.";
  }

  // ── locate line boundaries ──
  // Find the character position of the start of start_line
  auto line_start_pos = [&](int line) -> std::string::size_type {
    if (line <= 1) {
      return 0;
    }
    std::string::size_type pos = 0;
    for (int i = 0; i < line - 1; ++i) {
      pos = file_content.find('\n', pos);
      if (pos == std::string::npos) {
        return file_content.size();
      }
      ++pos;  // skip past the newline
    }
    return pos;
  };

  std::string::size_type start_pos = line_start_pos(start_line);
  std::string::size_type end_pos = line_start_pos(end_line + 1);

  // ── perform replacement ──
  std::string new_content;
  new_content.reserve(file_content.size() + content.size() + 1);
  new_content.append(file_content, 0, start_pos);
  new_content.append(content);
  if (!content.empty() && content.back() != '\n') {
    new_content.append("\n");
  }
  if (end_pos < file_content.size()) {
    new_content.append(file_content, end_pos, std::string::npos);
  }

  // ── show diff between original file and modified content ──
  {
    ai::utils::TempFile temp("",
                             std::filesystem::path(path).filename().string());
    ai::utils::write_file(temp.path(), new_content);
    using namespace subprocess::named_arguments;
    using subprocess::run;
    if (0 == run(std::string("which"), "delta", std_out > devnull,
                 std_err > devnull)) {
      run("delta", "--paging=never", path, temp.path());
    } else {
      run("diff", "-U0", "--color=always", path, temp.path());
    }
  }

  // ── persist modified content back to the original file ──
  if (!ai::utils::write_file(path, new_content)) {
    return "Failed to write to file: " + path;
  }

  return "Successfully replaced lines " + std::to_string(start_line) + "-" +
         std::to_string(end_line) + " in " + path;
}

std::string execute_file(nlohmann::json const& args) {
  if (!args.is_object()) {
    return "function execute_file arguments is invalid: expected a JSON "
           "object.";
  }
  auto path_opt = resolve_path(args);
  if (!path_opt.has_value()) {
    return "function execute_file arguments is invalid: missing required "
           "parameter \"path\".";
  }
  if (path_opt->empty()) {
    return "function execute_file arguments is invalid: \"path\" must be a "
           "string.";
  }

  std::string path = std::move(*path_opt);
  path = expand_tilde(path);

  // Build the command vector
  std::vector<std::string> cmd_args;
  cmd_args.push_back(path);
  if (args.contains("args") && args["args"].is_array()) {
    for (auto const& a : args["args"]) {
      if (a.is_string()) {
        cmd_args.push_back(a.get<std::string>());
      }
    }
  }

  // Execute and capture output
  using namespace subprocess::named_arguments;
  auto timeout_val =
      args.contains("timeout") && args["timeout"].is_number_integer()
          ? args["timeout"].get<int>()
          : timeout_infinite;
  std::string working_directory =
      args.contains("working_directory") &&
              args["working_directory"].is_string()
          ? args["working_directory"].get<std::string>()
          : "";

  std::string args_str;
  for (size_t i = 0; i < cmd_args.size(); ++i) {
    if (i > 0) {
      args_str += " ";
    }
    args_str += cmd_args[i];
  }
  print_toolcall_log("execute_file",
                     {{"path", path},
                      {"working_directory", working_directory},
                      {"timeout", timeout_val == $timeout_infinite
                                      ? "infinite"
                                      : std::to_string(timeout_val)},
                      {"args", args_str}});

  auto start = std::chrono::steady_clock::now();
  auto [exit_code, out_buf, err_buf] = subprocess::capture_run(
      cmd_args, timeout = timeout_val, cwd = working_directory);
  auto elapsed = std::chrono::steady_clock::now() - start;

  std::string result;
  result += "Exit code: " + std::to_string(exit_code) + "\n";

  std::string out_str = out_buf.to_string();
  std::string err_str = err_buf.to_string();

  if (!out_str.empty()) {
    result += "stdout:\n" + out_str;
    if (!err_str.empty()) {
      result += "\n";
    }
  }
  if (!err_str.empty()) {
    result += "stderr:\n" + err_str;
  }

  if (exit_code != 0 && timeout_val != timeout_infinite &&
      elapsed >= std::chrono::seconds(timeout_val)) {
    result +=
        ("\n\n\nError: command timed out after " + std::to_string(timeout_val) +
         " seconds. Exit code: " + std::to_string(exit_code));
  } else {
    if (out_str.empty() && err_str.empty()) {
      result += "(no output)";
    }
  }

  return result;
}

std::string_view get_filesystem_tools() { return filesystem_tools_json_str; }

void regist_filesystem_tools() {
  regist_tool_calls("read_file", read_file);
  regist_tool_calls("read_multiple_files", read_multiple_files);
  regist_tool_calls("write_file", write_file);
  regist_tool_calls("edit_file", edit_file);
  regist_tool_calls("create_directory", create_directory);
  regist_tool_calls("list_directory", list_directory);
  regist_tool_calls("directory_tree", directory_tree);
  regist_tool_calls("move_file", move_file);
  regist_tool_calls("search_files", search_files);
  regist_tool_calls("get_file_info", get_file_info);
  regist_tool_calls("disk_space_info", disk_space_info);
  regist_tool_calls("execute_file", execute_file);
  regist_tool_calls("replace_lines", replace_lines);
}

// Self-register the category at static-init time
static bool _filesystem_tool_category_registered = regist_tool_category(
    "filesystem", get_filesystem_tools, regist_filesystem_tools);
