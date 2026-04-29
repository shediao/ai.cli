#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <nlohmann/json.hpp>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <subprocess/subprocess.hpp>

#include "ai/args.h"
#include "filesystem_tools_json.h"
#include "./glob.hpp"
#include "ai/logging.h"
#include "ai/tool_calls.h"
#include "ai/utils.h"

std::string read_file(nlohmann::json const& args) {
  LOG(INFO) << "call read_file(" << args.dump() << ")";
  if (!args.is_object()) {
    return "function read_file arguments is invalid: expected a JSON object.";
  }
  if (!args.contains("path") && !args.contains("file")) {
    return "function read_file arguments is invalid: missing required "
           "parameter \"path\" or \"file\".";
  }
  if (args.contains("path") && !args["path"].is_string()) {
    return "function read_file arguments is invalid: \"path\" must be a "
           "string.";
  }
  if (args.contains("file") && !args["file"].is_string()) {
    return "function read_file arguments is invalid: \"file\" must be a "
           "string.";
  }
  std::string path = [](nlohmann::json const& args) -> std::string {
    if (!args.is_object()) {
      return "";
    }
    if (args.contains("path") && args["path"].is_string()) {
      return args["path"].get<std::string>();
    }
    if (args.contains("file") && args["file"].is_string()) {
      return args["file"].get<std::string>();
    }
    return "";
  }(args);
  if (path.empty()) {
    return "function read_file arguments is invalid.";
  }
  std::ifstream in(path);
  if (!in.is_open()) {
    return path + " is not exists.";
  }
  std::string content{std::istreambuf_iterator<char>(in),
                      std::istreambuf_iterator<char>()};
  if (content.empty()) {
    return path + " is empty.";
  }

  bool has_offset =
      args.contains("offset") && args["offset"].is_number_integer();
  bool has_limit = args.contains("limit") && args["limit"].is_number_integer();

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
  LOG(INFO) << "call read_multiple_files(" << args.dump() << ")";
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
  for (auto const& p : args["paths"]) {
    if (p.is_string()) {
      paths.push_back(p.get<std::string>());
    }
  }
  std::string contents;
  for (auto const& path : paths) {
    if (!contents.empty()) {
      contents += "\n------\n";
    }
    std::ifstream in(path);
    if (in.is_open()) {
      std::string file_content{std::istreambuf_iterator<char>(in),
                               std::istreambuf_iterator<char>()};
      contents += path;
      contents += "\n";
      if (!file_content.empty()) {
        contents += file_content;
      } else {
        contents += "(empty)";
      }
    } else {
      contents += path + " (failed to read)";
    }
  }
  return contents;
}

std::string write_file(nlohmann::json const& args) {
  LOG(INFO) << "call write_file(" << args.dump() << ")";
  if (!args.is_object()) {
    return "function write_file arguments is invalid: expected a JSON "
           "object.";
  }
  if (!args.contains("path")) {
    return "function write_file arguments is invalid: missing required "
           "parameter \"path\".";
  }
  if (!args["path"].is_string()) {
    return "function write_file arguments is invalid: \"path\" must be a "
           "string.";
  }
  if (!args.contains("content")) {
    return "function write_file arguments is invalid: missing required "
           "parameter \"content\".";
  }
  if (!args["content"].is_string()) {
    return "function write_file arguments is invalid: \"content\" must be a "
           "string.";
  }
  std::string path = args["path"].get<std::string>();
  std::string content = args["content"].get<std::string>();
  std::ofstream out(path);
  if (out.is_open()) {
    out.write(content.data(), content.size());
    out.flush();
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
  LOG(INFO) << "call edit_file(" << args.dump() << ")";

  // --- argument validation ---
  if (!args.is_object()) {
    return "function edit_file arguments is invalid: expected a JSON object.";
  }
  if (!args.contains("path")) {
    return "function edit_file arguments is invalid: missing required "
           "parameter \"path\".";
  }
  if (!args["path"].is_string()) {
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

  std::string path = args["path"].get<std::string>();
  std::string diff = args["diff"].get<std::string>();

  // --- read original file ---
  std::ifstream in(path);
  if (!in.is_open()) {
    return "Failed to open file: " + path;
  }
  std::string file_content{std::istreambuf_iterator<char>(in),
                           std::istreambuf_iterator<char>()};
  in.close();

  // --- static label tables (avoid re-allocation every call) ---
  static const std::vector<std::string_view> search_labels{"\n<<<<<<< SEARCH\n",
                                                           "<<<<<<< SEARCH\n"};
  static const std::vector<std::string_view> replace_labels{
      "\n>>>>>>> REPLACE\n", "\n>>>>>>> REPLACE"};
  static const std::vector<std::string_view> split_labels{"\n=======\n"};

  // --- parse diff and apply each SEARCH/REPLACE block ---
  std::string_view diff_view(diff);
  size_t cursor = 0;

  while (true) {
    // 1. locate next SEARCH marker
    auto [search_pos, search_label] =
        find_by_lables(diff, cursor, search_labels);
    if (search_pos == std::string::npos) {
      break;  // no more blocks
    }

    // 2. locate SPLIT marker (natural order: SEARCH → SPLIT → REPLACE)
    auto [split_pos, split_label] =
        find_by_lables(diff, search_pos + search_label.size(), split_labels);
    if (split_pos == std::string::npos) {
      LOG(ERROR) << "not found label: '\\n=======\\n'";
      return "Failed to edit file " + path;
    }

    // 3. locate REPLACE marker after the split
    auto [replace_pos, replace_label] =
        find_by_lables(diff, split_pos + split_label.size(), replace_labels);
    if (replace_pos == std::string::npos) {
      LOG(ERROR) << "not found label: '>>>>>>> REPLACE'";
      return "Failed to edit file " + path;
    }

    // 4. extract search & replace strings (string_view avoids copies)
    std::string_view search =
        diff_view.substr(search_pos + search_label.size(),
                         split_pos - search_pos - search_label.size());
    std::string_view replace =
        diff_view.substr(split_pos + split_label.size(),
                         replace_pos - split_pos - split_label.size());

    // 5. apply the replacement
    size_t found = file_content.find(search);
    if (found == std::string::npos) {
      LOG(ERROR) << "Not Found: " << search;
      return "Failed edited file " + path;
    }
    file_content.replace(found, search.size(), replace);

    // 6. advance past this block for the next iteration
    cursor = replace_pos + replace_label.size();
  }

  // --- show diff between original file and modified content ---
  {
    TempFile temp("", std::filesystem::path(path).filename().string());
    if (std::ofstream ftemp(temp.path()); ftemp.is_open()) {
      ftemp.write(file_content.data(), file_content.size());
      ftemp.flush();
    }
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
  {
    std::ofstream out(path);
    if (!out.is_open()) {
      return "Failed to write to file: " + path;
    }
    out.write(file_content.data(), file_content.size());
    out.flush();
  }

  return "Successfully edited file " + path;
}

std::string create_directory(nlohmann::json const& args) {
  LOG(INFO) << "call create_directory(" << args.dump() << ")";
  if (!args.is_object()) {
    return "function create_directory arguments is invalid: expected a JSON "
           "object.";
  }
  if (!args.contains("path")) {
    return "function create_directory arguments is invalid: missing required "
           "parameter \"path\".";
  }
  if (!args["path"].is_string()) {
    return "function create_directory arguments is invalid: \"path\" must be "
           "a string.";
  }
  std::string path = args["path"].get<std::string>();
  std::error_code err;
  std::filesystem::create_directories(path, err);
  if (err) {
    return "Error: " + err.message();
  } else {
    return "Successfully created directory " + path;
  }
}

std::string list_directory(nlohmann::json const& args) {
  LOG(INFO) << "call list_directory(" << args.dump() << ")";
  if (!args.is_object()) {
    return "function list_directory arguments is invalid: expected a JSON "
           "object.";
  }
  if (!args.contains("path")) {
    return "function list_directory arguments is invalid: missing required "
           "parameter \"path\".";
  }
  if (!args["path"].is_string()) {
    return "function list_directory arguments is invalid: \"path\" must be a "
           "string.";
  }
  std::string path = args["path"].get<std::string>();
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
  LOG(INFO) << "call directory_tree(" << args.dump() << ")";
  if (!args.is_object()) {
    return "function directory_tree arguments is invalid: expected a JSON "
           "object.";
  }
  if (!args.contains("path")) {
    return "function directory_tree arguments is invalid: missing required "
           "parameter \"path\".";
  }
  if (!args["path"].is_string()) {
    return "function directory_tree arguments is invalid: \"path\" must be a "
           "string.";
  }
  std::string path = args["path"].get<std::string>();
  std::error_code err;
  if (!std::filesystem::exists(path, err) ||
      !std::filesystem::is_directory(path, err) || err) {
    return "Error: " + path + " not a directory or not exists";
  }
  return buildTree(path).dump(2);
}

std::string search_files(nlohmann::json const& args) {
  LOG(INFO) << "call search_files(" << args.dump() << ")";
  if (!args.is_object()) {
    return "function search_files arguments is invalid: expected a JSON "
           "object.";
  }
  if (!args.contains("path")) {
    return "function search_files arguments is invalid: missing required "
           "parameter \"path\".";
  }
  if (!args["path"].is_string()) {
    return "function search_files arguments is invalid: \"path\" must be a "
           "string.";
  }
  if (!args.contains("pattern")) {
    return "function search_files arguments is invalid: missing required "
           "parameter \"pattern\".";
  }
  if (!args["pattern"].is_string()) {
    return "function search_files arguments is invalid: \"pattern\" must be "
           "a string.";
  }
  std::string path = args["path"].get<std::string>();
  std::string pattern = args["pattern"].get<std::string>();

  bool recursive = false;
  if (args.contains("recursive") && args["recursive"].is_boolean()) {
    recursive = args["recursive"].get<bool>();
  }

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
  LOG(INFO) << "call get_file_info(" << args.dump() << ")";
  if (!args.is_object()) {
    return "function get_file_info arguments is invalid: expected a JSON "
           "object.";
  }
  if (!args.contains("path")) {
    return "function get_file_info arguments is invalid: missing required "
           "parameter \"path\".";
  }
  if (!args["path"].is_string()) {
    return "function get_file_info arguments is invalid: \"path\" must be a "
           "string.";
  }
  std::string path = args["path"].get<std::string>();
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
  info["last_modified"] = std::ctime(&last_modified);
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
  LOG(INFO) << "call disk_space_info(" << args.dump() << ")";
  if (!args.is_object()) {
    return "function disk_space_info arguments is invalid: expected a JSON "
           "object.";
  }
  if (!args.contains("path")) {
    return "function disk_space_info arguments is invalid: missing required "
           "parameter \"path\".";
  }
  if (!args["path"].is_string()) {
    return "function disk_space_info arguments is invalid: \"path\" must be "
           "a string.";
  }
  std::string path = args["path"].get<std::string>();
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
  LOG(INFO) << "call directory_tree(" << args.dump() << ")";
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
  std::string distination = args["distination"].get<std::string>();
  std::error_code err;
  std::filesystem::rename(source, distination, err);

  if (err) {
    return "Error: " + err.message();
  } else {
    return "Successfully moved " + source + " to " + distination;
  }
}

std::string replace_lines(nlohmann::json const& args) {
  LOG(INFO) << "call replace_lines(" << args.dump() << ")";

  // ── argument validation ──
  if (!args.is_object()) {
    return "function replace_lines arguments is invalid: expected a JSON "
           "object.";
  }
  if (!args.contains("path")) {
    return "function replace_lines arguments is invalid: missing required "
           "parameter \"path\".";
  }
  if (!args["path"].is_string()) {
    return "function replace_lines arguments is invalid: \"path\" must be a "
           "string.";
  }
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

  std::string path = args["path"].get<std::string>();
  int start_line = args["start_line"].get<int>();
  int end_line = args["end_line"].get<int>();
  std::string content = args["content"].get<std::string>();

  if (start_line < 1) {
    return "function replace_lines: \"start_line\" must be >= 1 (1-indexed), "
           "got " +
           std::to_string(start_line);
  }
  if (end_line < start_line) {
    return "function replace_lines: \"end_line\" (" +
           std::to_string(end_line) +
           ") must be >= \"start_line\" (" + std::to_string(start_line) + ")";
  }

  // ── read original file ──
  std::ifstream in(path);
  if (!in.is_open()) {
    return "Failed to open file: " + path;
  }
  std::string file_content{std::istreambuf_iterator<char>(in),
                           std::istreambuf_iterator<char>()};
  in.close();

  // Count total lines
  int total_lines =
      static_cast<int>(std::count(file_content.begin(), file_content.end(), '\n'));
  if (!file_content.empty() && file_content.back() != '\n') {
    ++total_lines;
  }

  if (start_line > total_lines) {
    return "function replace_lines: \"start_line\" " + std::to_string(start_line) +
           " is out of range. File \"" + path + "\" has only " +
           std::to_string(total_lines) + " lines.";
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
  new_content.reserve(file_content.size() + content.size());
  new_content.append(file_content, 0, start_pos);
  new_content.append(content);
  if (end_pos < file_content.size()) {
    new_content.append(file_content, end_pos, std::string::npos);
  }

  // ── show diff between original file and modified content ──
  {
    TempFile temp("", std::filesystem::path(path).filename().string());
    if (std::ofstream ftemp(temp.path()); ftemp.is_open()) {
      ftemp.write(new_content.data(), new_content.size());
      ftemp.flush();
    }
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
  {
    std::ofstream out(path);
    if (!out.is_open()) {
      return "Failed to write to file: " + path;
    }
    out.write(new_content.data(), new_content.size());
    out.flush();
  }

  return "Successfully replaced lines " + std::to_string(start_line) + "-" +
         std::to_string(end_line) + " in " + path;
}

std::string execute_file(nlohmann::json const& args) {
  LOG(INFO) << "call execute_file(" << args.dump() << ")";
  if (!args.is_object()) {
    return "function execute_file arguments is invalid: expected a JSON "
           "object.";
  }
  if (!args.contains("path")) {
    return "function execute_file arguments is invalid: missing required "
           "parameter \"path\".";
  }
  if (!args["path"].is_string()) {
    return "function execute_file arguments is invalid: \"path\" must be a "
           "string.";
  }

  std::string path = args["path"].get<std::string>();

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
  auto [exit_code, out_buf, err_buf] = subprocess::capture_run(cmd_args);

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

  if (out_str.empty() && err_str.empty()) {
    result += "(no output)";
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
