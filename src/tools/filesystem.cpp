

#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>

#include "../args.h"
#include "../tools_call.h"
#include "filesystem_tools_json.h"
#include "nlohmann/json.hpp"

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
    if (args.is_object() && args.contains("paths") &&
        args["paths"].is_array()) {
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
                break;
            }
            auto [split_lable_pos, split_lable] =
                find_by_lables(diff, search_lable_pos, split_lables);
            if (split_lable_pos == std::string::npos ||
                split_lable_pos > replace_lable_pos) {
                break;
            }

            auto search = diff.substr(
                search_lable_pos + search_lable.size(),
                split_lable_pos - search_lable_pos - search_lable.size());
            auto replace = diff.substr(
                split_lable_pos + split_lable.size(),
                replace_lable_pos - split_lable_pos - split_lable.size());
            auto search_pos = file_content.find(search);

            if (search_pos != std::string::npos) {
                file_content.replace(search_pos, search.size(), replace);
            }
            auto [search_lable_pos2, search_lable2] =
                find_by_lables(file_content, search_lable_pos, search_lables);
            search_lable_pos = search_lable_pos2;
            search_lable = search_lable2;
        }

        std::ofstream out(path);
        if (out.is_open()) {
            out.write(file_content.data(), file_content.size());
            out.flush();
        }
        return "edit_file 已经成功修改文件 " + path;
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
        for (auto const& entry :
             std::filesystem::directory_iterator(path, err)) {
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
        for (auto const& entry :
             std::filesystem::directory_iterator(path, err)) {
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
    regist_tools_call("read_file", read_file);
    regist_tools_call("read_multiple_files", read_multiple_files);
    regist_tools_call("write_file", write_file);
    regist_tools_call("edit_file", edit_file);
    regist_tools_call("create_directory", create_directory);
    regist_tools_call("list_directory", list_directory);
    regist_tools_call("directory_tree", directory_tree);
    regist_tools_call("move_file", move_file);
}
