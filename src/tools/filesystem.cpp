

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
  if (args.is_object() && args.contains("path") && args["path"].is_string()) {
    std::string path = args["path"].get<std::string>();
    std::ifstream in(path);
    if (in.is_open()) {
      std::string content{std::istreambuf_iterator<char>(in),
                          std::istreambuf_iterator<char>()};
      if (!content.empty()) {
        bool has_limit = args.contains("limit") && args["limit"].is_number_integer();
        bool has_offset = args.contains("offset") && args["offset"].is_number_integer();

        if (has_limit || has_offset) {
          int limit = has_limit ? args["limit"].get<int>() : -1;
          int offset = has_offset ? args["offset"].get<int>() : 1;
          if (offset < 1) offset = 1;

          std::vector<std::string> lines;
          std::istringstream iss(content);
          std::string line;
          while (std::getline(iss, line)) {
            lines.push_back(line);
          }

          if (offset > static_cast<int>(lines.size())) {
            return path + " has only " + std::to_string(lines.size()) +
                   " lines, offset " + std::to_string(offset) +
                   " is out of range.";
          }

          std::string result;
          int start = offset - 1;
          int end = (limit > 0) ? std::min(start + limit, static_cast<int>(lines.size()))
                               : static_cast<int>(lines.size());
          for (int i = start; i < end; ++i) {
            if (i > start) result += '\n';
            result += lines[i];
          }
          return result;
        }

        return content;
      } else {
        return path + " is empty.";
      }
    }
    return path + " is not exists.";
  }
  return "tool_calls read_file arguments is invalid.";
}

std::string read_multiple_files(nlohmann::json const& args) {
  LOG(INFO) << "call read_multiple_files(" << args.dump() << ")";
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
        contents += "\n------\n";
        if (!contents.empty()) {
          contents += path;
          contents += "\n";
          contents += file_content;
        } else {
          contents += path + " is empty.";
        }
      }
    }
    return contents;
  }
  return "tool_calls read_multiple_files arguments is invalid.";
}

std::string write_file(nlohmann::json const& args) {
  LOG(INFO) << "call write_file(" << args.dump() << ")";
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
  return "tool_calls write_file arguments is invalid.";
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

std::string edit_file(nlohmann::json const& args) {
  LOG(INFO) << "call edit_file(" << args.dump() << ")";
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
        LOG(ERROR) << "not found lable: '>>>>>>> REPLACE'";
        return "User cancel edit file: " + path;
        break;
      }
      auto [split_lable_pos, split_lable] =
          find_by_lables(diff, search_lable_pos, split_lables);
      if (split_lable_pos == std::string::npos ||
          split_lable_pos > replace_lable_pos) {
        LOG(ERROR) << "not found lable: '======='";
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
      } else {
        LOG(ERROR) << "Not Found: " << search;
      }
      auto [search_lable_pos2, search_lable2] =
          find_by_lables(diff, replace_lable_pos, search_lables);
      search_lable_pos = search_lable_pos2;
      search_lable = search_lable2;
    }

    subprocess::buffer diff_str;
    {
      TempFile temp("", std::filesystem::path(path).filename().string());

      if (std::ofstream ftemp(temp.path()); ftemp.is_open()) {
        ftemp.write(file_content.data(), file_content.size());
        ftemp.flush();
      }
      using namespace subprocess::named_arguments;
      using subprocess::run;
      run("diff", "-U0", "--color=never", path, temp.path(),
          std_out > diff_str);
      if (0 == run(std::string("which"), "delta", std_out > devnull,
                   std_err > devnull)) {
        run("delta", "--paging=never", path, temp.path());
      } else {
        run("diff", "-U0", "--color=always", path, temp.path());
      }
    }

    if (std::ofstream out(path); out.is_open()) {
      out.write(file_content.data(), file_content.size());
      out.flush();
    }
    if (diff_str.empty()) {
      return "Successfully edited file " + path;
    } else {
      return diff_str.to_string();
    }
  }
  return "tool_calls edit_file arguments is invalid.";
}

std::string create_directory(nlohmann::json const& args) {
  LOG(INFO) << "call create_directory(" << args.dump() << ")";
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
        if (!ret.empty()) ret += '\n';
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
      if (!ts.empty() && ts.back() == '\n') ts.pop_back();
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

std::string_view get_filesystem_tools() {
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
  regist_tool_calls("search_files", search_files);
  regist_tool_calls("get_file_info", get_file_info);
}

// Self-register the category at static-init time
static bool _filesystem_tool_category_registered = regist_tool_category(
    "filesystem", get_filesystem_tools, regist_filesystem_tools);
