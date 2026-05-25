#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <vector>

#include "ai/function.h"
#include "ai/utils.h"
#include "base/file.h"

extern std::string expand_tilde(std::string const& path);

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
