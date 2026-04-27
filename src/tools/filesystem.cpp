#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <nlohmann/json.hpp>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <subprocess/subprocess.hpp>

#include "args.h"
#include "filesystem_tools_json.h"
#include "logging.h"
#include "tool_calls.h"
#include "utils.h"

std::string read_file(nlohmann::json const& args) {
  LOG(INFO) << "call read_file(" << args.dump() << ")";
  if (!args.is_object()) {
    return "tool_calls read_file arguments is invalid: expected a JSON object.";
  }
  if (!args.contains("path") && !args.contains("file")) {
    return "tool_calls read_file arguments is invalid: missing required "
           "parameter \"path\" or \"file\".";
  }
  if (args.contains("path") && !args["path"].is_string()) {
    return "tool_calls read_file arguments is invalid: \"path\" must be a "
           "string.";
  }
  if (args.contains("file") && !args["file"].is_string()) {
    return "tool_calls read_file arguments is invalid: \"file\" must be a "
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
    return "tool_calls read_file arguments is invalid.";
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
    return "tool_calls read_multiple_files arguments is invalid: expected a "
           "JSON object.";
  }
  if (!args.contains("paths")) {
    return "tool_calls read_multiple_files arguments is invalid: missing "
           "required parameter \"paths\".";
  }
  if (!args["paths"].is_array()) {
    return "tool_calls read_multiple_files arguments is invalid: \"paths\" "
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
    return "tool_calls write_file arguments is invalid: expected a JSON "
           "object.";
  }
  if (!args.contains("path")) {
    return "tool_calls write_file arguments is invalid: missing required "
           "parameter \"path\".";
  }
  if (!args["path"].is_string()) {
    return "tool_calls write_file arguments is invalid: \"path\" must be a "
           "string.";
  }
  if (!args.contains("content")) {
    return "tool_calls write_file arguments is invalid: missing required "
           "parameter \"content\".";
  }
  if (!args["content"].is_string()) {
    return "tool_calls write_file arguments is invalid: \"content\" must be a "
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
    return "tool_calls edit_file arguments is invalid: expected a JSON object.";
  }
  if (!args.contains("path")) {
    return "tool_calls edit_file arguments is invalid: missing required "
           "parameter \"path\".";
  }
  if (!args["path"].is_string()) {
    return "tool_calls edit_file arguments is invalid: \"path\" must be a "
           "string.";
  }
  if (!args.contains("diff")) {
    return "tool_calls edit_file arguments is invalid: missing required "
           "parameter \"diff\".";
  }
  if (!args["diff"].is_string()) {
    return "tool_calls edit_file arguments is invalid: \"diff\" must be a "
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
  static const std::vector<std::string_view> search_labels{"<<<<<<< SEARCH\n",
                                                           "<<<<<<< SEARCH"};
  static const std::vector<std::string_view> replace_labels{"\n>>>>>>> REPLACE",
                                                            ">>>>>>> REPLACE"};
  static const std::vector<std::string_view> split_labels{
      "\n=======\n", "=======\n", "\n=======", "======="};

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
      LOG(ERROR) << "not found label: '======='";
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
    return "tool_calls create_directory arguments is invalid: expected a JSON "
           "object.";
  }
  if (!args.contains("path")) {
    return "tool_calls create_directory arguments is invalid: missing required "
           "parameter \"path\".";
  }
  if (!args["path"].is_string()) {
    return "tool_calls create_directory arguments is invalid: \"path\" must be "
           "a string.";
  }
  if (args.is_object() && args.contains("path") && args["path"].is_string()) {
    std::string path = args["path"].get<std::string>();
    std::error_code err;
    std::filesystem::create_directories(path, err);
    if (err) {
      return "Error: " + err.message();
    } else {
      return "Successfully created directory " + path;
    }
  }
  return "tool_calls create_directory arguments is invalid.";
}

std::string list_directory(nlohmann::json const& args) {
  LOG(INFO) << "call list_directory(" << args.dump() << ")";
  if (!args.is_object()) {
    return "tool_calls list_directory arguments is invalid: expected a JSON "
           "object.";
  }
  if (!args.contains("path")) {
    return "tool_calls list_directory arguments is invalid: missing required "
           "parameter \"path\".";
  }
  if (!args["path"].is_string()) {
    return "tool_calls list_directory arguments is invalid: \"path\" must be a "
           "string.";
  }
  if (args.is_object() && args.contains("path") && args["path"].is_string()) {
    std::string path = args["path"].get<std::string>();
    std::error_code err;
    if (!std::filesystem::exists(path, err) ||
        !std::filesystem::is_directory(path, err) || err) {
      return "Error: " + err.message();
    }
    std::string ret;
    for (auto const& entry : std::filesystem::directory_iterator(path, err)) {
      if (entry.is_directory()) {
        ret += "\n[DIR]" + entry.path().string();
      } else {
        ret += "\n[FILE]" + entry.path().string();
      }
    }
    return ret;
  }
  return "tool_calls list_directory arguments is invalid.";
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
    return "tool_calls directory_tree arguments is invalid: expected a JSON "
           "object.";
  }
  if (!args.contains("path")) {
    return "tool_calls directory_tree arguments is invalid: missing required "
           "parameter \"path\".";
  }
  if (!args["path"].is_string()) {
    return "tool_calls directory_tree arguments is invalid: \"path\" must be a "
           "string.";
  }
  if (args.is_object() && args.contains("path") && args["path"].is_string()) {
    std::string path = args["path"].get<std::string>();
    std::error_code err;
    if (!std::filesystem::exists(path, err) ||
        !std::filesystem::is_directory(path, err) || err) {
      return "Error: " + path + " not a directory or not exists";
    }
    return buildTree(path).dump(2);
  }
  return "tool_calls directory_tree arguments is invalid.";
}

std::string search_files(nlohmann::json const& args) {
  LOG(INFO) << "call search_files(" << args.dump() << ")";
  if (!args.is_object()) {
    return "tool_calls search_files arguments is invalid: expected a JSON "
           "object.";
  }
  if (!args.contains("path")) {
    return "tool_calls search_files arguments is invalid: missing required "
           "parameter \"path\".";
  }
  if (!args["path"].is_string()) {
    return "tool_calls search_files arguments is invalid: \"path\" must be a "
           "string.";
  }
  if (!args.contains("pattern")) {
    return "tool_calls search_files arguments is invalid: missing required "
           "parameter \"pattern\".";
  }
  if (!args["pattern"].is_string()) {
    return "tool_calls search_files arguments is invalid: \"pattern\" must be "
           "a string.";
  }
  if (args.is_object() && args.contains("path") && args["path"].is_string() &&
      args.contains("pattern") && args["pattern"].is_string()) {
    std::string path = args["path"].get<std::string>();
    std::string pattern = args["pattern"].get<std::string>();

    // Convert pattern to lowercase for case-insensitive matching
    std::string pattern_lower = pattern;
    std::transform(pattern_lower.begin(), pattern_lower.end(),
                   pattern_lower.begin(), ::tolower);

    std::error_code err;
    if (!std::filesystem::exists(path, err) ||
        !std::filesystem::is_directory(path, err) || err) {
      return "Error: " + path + " is not a valid directory (" + err.message() +
             ")";
    }

    std::string ret;
    for (auto const& entry :
         std::filesystem::recursive_directory_iterator(path, err)) {
      std::string filename = entry.path().filename().string();
      std::string filename_lower = filename;
      std::transform(filename_lower.begin(), filename_lower.end(),
                     filename_lower.begin(), ::tolower);

      if (filename_lower.find(pattern_lower) != std::string::npos) {
        if (!ret.empty()) {
          ret += '\n';
        }
        if (entry.is_directory(err)) {
          ret += "[DIR] " + entry.path().string();
        } else {
          ret += "[FILE] " + entry.path().string();
        }
      }
    }

    if (ret.empty()) {
      return "No files or directories matching \"" + pattern + "\" found in " +
             path;
    }
    return ret;
  }
  return "tool_calls search_files arguments is invalid.";
}

std::string get_file_info(nlohmann::json const& args) {
  LOG(INFO) << "call get_file_info(" << args.dump() << ")";
  if (!args.is_object()) {
    return "tool_calls get_file_info arguments is invalid: expected a JSON "
           "object.";
  }
  if (!args.contains("path")) {
    return "tool_calls get_file_info arguments is invalid: missing required "
           "parameter \"path\".";
  }
  if (!args["path"].is_string()) {
    return "tool_calls get_file_info arguments is invalid: \"path\" must be a "
           "string.";
  }
  if (args.is_object() && args.contains("path") && args["path"].is_string()) {
    std::string path = args["path"].get<std::string>();
    std::error_code err;

    if (!std::filesystem::exists(path, err)) {
      return "Error: " + path + " does not exist.";
    }

    nlohmann::json info;
    info["path"] = std::filesystem::absolute(path, err).string();

    // Type
    if (std::filesystem::is_regular_file(path, err)) {
      info["type"] = "file";
      info["size"] = std::filesystem::file_size(path, err);
    } else if (std::filesystem::is_directory(path, err)) {
      info["type"] = "directory";
    } else if (std::filesystem::is_symlink(path, err)) {
      info["type"] = "symlink";
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
      s += (p & std::filesystem::perms::owner_read) !=
                   std::filesystem::perms::none
               ? r
               : '-';
      s += (p & std::filesystem::perms::owner_write) !=
                   std::filesystem::perms::none
               ? w
               : '-';
      s += (p & std::filesystem::perms::owner_exec) !=
                   std::filesystem::perms::none
               ? x
               : '-';
      return s;
    };
    std::string perm_str;
    perm_str += to_perm_str(perms, 'r', 'w', 'x');
    info["permissions"] = perm_str;

    return info.dump(2);
  }
  return "tool_calls get_file_info arguments is invalid.";
}

std::string move_file(nlohmann::json const& args) {
  LOG(INFO) << "call directory_tree(" << args.dump() << ")";
  if (!args.is_object()) {
    return "tool_calls move_file arguments is invalid: expected a JSON object.";
  }
  if (!args.contains("source")) {
    return "tool_calls move_file arguments is invalid: missing required "
           "parameter \"source\".";
  }
  if (!args["source"].is_string()) {
    return "tool_calls move_file arguments is invalid: \"source\" must be a "
           "string.";
  }
  if (!args.contains("distination")) {
    return "tool_calls move_file arguments is invalid: missing required "
           "parameter \"distination\".";
  }
  if (!args["distination"].is_string()) {
    return "tool_calls move_file arguments is invalid: \"distination\" must be "
           "a string.";
  }
  if (args.is_object() && args.contains("source") &&
      args["source"].is_string() && args.contains("distination") &&
      args["distination"].is_string()) {
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
  return "tool_calls move_file arguments is invalid.";
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
}

// Self-register the category at static-init time
static bool _filesystem_tool_category_registered = regist_tool_category(
    "filesystem", get_filesystem_tools, regist_filesystem_tools);
