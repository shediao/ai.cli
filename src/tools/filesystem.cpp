

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <subprocess/subprocess.hpp>

#include "../args.h"
#include "../tool_calls.h"
#include "../utils.h"
#include "filesystem_tools_json.h"

std::optional<std::string> read_file(nlohmann::json const& args) {
  if (AiArgs::instance().debug) {
    std::cout << "call read_file(" << args.dump() << ")\n";
  }
  if (args.is_object() && args.contains("path") && args["path"].is_string()) {
    std::string path = args["path"].get<std::string>();
    std::ifstream in(path);
    if (in.is_open()) {
      std::string content{std::istreambuf_iterator<char>(in),
                          std::istreambuf_iterator<char>()};
      return content;
    }
  }
  return std::nullopt;
}

std::optional<std::string> read_multiple_files(nlohmann::json const& args) {
  if (AiArgs::instance().debug) {
    std::cout << "call read_multiple_files(" << args.dump() << ")\n";
  }
  if (args.is_object() && args.contains("paths") && args["paths"].is_array()) {
    std::vector<std::string> paths;
    for (auto const& p : args["paths"]) {
      if (p.is_string()) {
        paths.push_back(p.get<std::string>());
      }
    }
    std::string contents;
    for (auto const& path : paths) {
      std::ifstream in(path);
      if (in.is_open()) {
        std::string file_content{std::istreambuf_iterator<char>(in),
                                 std::istreambuf_iterator<char>()};
        if (!contents.empty()) {
          contents += "\n------\n";
        }
        contents += path;
        contents += "\n";
        contents += file_content;
      }
    }
    if (contents.empty()) {
      return std::nullopt;
    } else {
      return contents;
    }
  }
  return std::nullopt;
}

std::optional<std::string> write_file(nlohmann::json const& args) {
  if (AiArgs::instance().debug) {
    std::cout << "call write_file(" << args.dump() << ")\n";
  }
  if (args.is_object() && args.contains("path") && args["path"].is_string() &&
      args.contains("content") && args["content"].is_string()) {
    std::string path = args["path"].get<std::string>();
    std::string content = args["content"].get<std::string>();
    std::ofstream out(path);
    if (out.is_open()) {
      out.write(content.data(), content.size());
      out.flush();
    }
    return "Successfully wrote to " + path;
  }
  return std::nullopt;
}

std::pair<size_t, std::string_view> find_by_lables(
    const std::string& str, size_t start_pos,
    std::vector<std::string_view> const& lables) {
  auto search_lable_pos = std::string::npos;
  auto it = std::find_if(
      begin(lables), end(lables),
      [&str, &search_lable_pos, start_pos](std::string_view lable) {
        search_lable_pos = str.find(lable, start_pos);
        return search_lable_pos != std::string::npos;
      });
  if (it == end(lables)) {
    return {std::string::npos, ""};
  }
  std::string_view lable{*it};
  return {search_lable_pos, lable};
}

std::optional<std::string> edit_file(nlohmann::json const& args) {
  if (AiArgs::instance().debug) {
    std::cout << "call edit_file(" << args.dump() << ")\n";
  }
  if (args.is_object() && args.contains("path") && args["path"].is_string() &&
      args.contains("diff") && args["diff"].is_string()) {
    std::string path = args["path"].get<std::string>();
    std::string diff = args["diff"].get<std::string>();
    std::ifstream in(path);
    std::string file_content{std::istreambuf_iterator<char>(in),
                             std::istreambuf_iterator<char>()};
    in.close();
    std::vector<std::string_view> search_lables{"<<<<<<< SEARCH\n",
                                                "<<<<<<< SEARCH"};
    std::vector<std::string_view> replace_lables{"\n>>>>>>> REPLACE",
                                                 ">>>>>>> REPLACE"};
    std::vector<std::string_view> split_lables{"\n=======\n", "=======\n",
                                               "\n=======", "======="};
    auto [search_lable_pos, search_lable] =
        find_by_lables(diff, 0, search_lables);
    while (search_lable_pos != std::string::npos) {
      auto [replace_lable_pos, replace_lable] =
          find_by_lables(diff, search_lable_pos, replace_lables);
      if (replace_lable_pos == std::string::npos) {
        std::cerr << "not found lable: '>>>>>>> REPLACE'" << '\n';
        return "User cancel edit file: " + path;
        break;
      }
      auto [split_lable_pos, split_lable] =
          find_by_lables(diff, search_lable_pos, split_lables);
      if (split_lable_pos == std::string::npos ||
          split_lable_pos > replace_lable_pos) {
        std::cerr << "not found lable: '======='" << '\n';
        return "User cancel edit file: " + path;
        break;
      }

      auto search =
          diff.substr(search_lable_pos + search_lable.size(),
                      split_lable_pos - search_lable_pos - search_lable.size());
      auto replace =
          diff.substr(split_lable_pos + split_lable.size(),
                      replace_lable_pos - split_lable_pos - split_lable.size());
      auto search_pos = file_content.find(search);

      if (search_pos != std::string::npos) {
        file_content.replace(search_pos, search.size(), replace);
      }
      auto [search_lable_pos2, search_lable2] =
          find_by_lables(diff, replace_lable_pos, search_lables);
      search_lable_pos = search_lable_pos2;
      search_lable = search_lable2;
    }

    std::vector<char> diff_str;
    {
      TempFile temp("", std::filesystem::path(path).filename().string());

      if (std::ofstream ftemp(temp.path()); ftemp.is_open()) {
        ftemp.write(file_content.data(), file_content.size());
        ftemp.flush();
      }
      using namespace process::named_arguments;
      process::run({"diff", "-U0", "--color=never", path, temp.path()},
                   std_out > diff_str);
      if (0 == process::run({"which", "delta"}, std_out > devnull,
                            std_err > devnull)) {
        process::run({"delta", "--paging=never", path, temp.path()});
      } else {
        process::run({"diff", "-U0", "--color=always", path, temp.path()});
      }
    }

    if (std::ofstream out(path); out.is_open()) {
      out.write(file_content.data(), file_content.size());
      out.flush();
    }
    if (diff_str.empty()) {
      return "Successfully edited file " + path;
    } else {
      return std::string{begin(diff_str), end(diff_str)};
    }
  }
  return std::nullopt;
}

std::optional<std::string> create_directory(nlohmann::json const& args) {
  if (AiArgs::instance().debug) {
    std::cout << "call create_directory(" << args.dump() << ")\n";
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
  return std::nullopt;
}

std::optional<std::string> list_directory(nlohmann::json const& args) {
  if (AiArgs::instance().debug) {
    std::cout << "call list_directory(" << args.dump() << ")\n";
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
  return std::nullopt;
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

std::optional<std::string> directory_tree(nlohmann::json const& args) {
  if (AiArgs::instance().debug) {
    std::cout << "call directory_tree(" << args.dump() << ")\n";
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
  return std::nullopt;
}

std::optional<std::string> move_file(nlohmann::json const& args) {
  if (AiArgs::instance().debug) {
    std::cout << "call directory_tree(" << args.dump() << ")\n";
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
  return std::nullopt;
}

const std::string_view get_filesystem_tools() {
  return filesystem_tools_json_str;
}

void regist_filesystem_tools() {
  regist_tool_calls("read_file", read_file);
  regist_tool_calls("read_multiple_files", read_multiple_files);
  regist_tool_calls("write_file", write_file);
  regist_tool_calls("edit_file", edit_file);
  regist_tool_calls("create_directory", create_directory);
  regist_tool_calls("list_directory", list_directory);
  regist_tool_calls("directory_tree", directory_tree);
  regist_tool_calls("move_file", move_file);
}
