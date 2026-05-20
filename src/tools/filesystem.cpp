#include <environment/environment.hpp>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>

#include "ai/function.h"
#include "filesystem_tools_json.h"

std::string expand_tilde(std::string const& path) {
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
std::optional<std::string> resolve_path(nlohmann::json const& args) {
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

std::string append_prefix_per_line(std::string_view str,
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

// Tool call function declarations (implementations in filesystem/*.cpp)
std::string read_file(nlohmann::json const& args);
std::string read_multiple_files(nlohmann::json const& args);
std::string write_file(nlohmann::json const& args);
std::string edit_file(nlohmann::json const& args);
std::string create_directory(nlohmann::json const& args);
std::string list_directory(nlohmann::json const& args);
std::string directory_tree(nlohmann::json const& args);
std::string move_file(nlohmann::json const& args);
std::string search_files(nlohmann::json const& args);
std::string get_file_info(nlohmann::json const& args);
std::string disk_space_info(nlohmann::json const& args);
std::string execute_file(nlohmann::json const& args);
std::string replace_lines(nlohmann::json const& args);

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
