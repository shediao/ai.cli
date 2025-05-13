

#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>

#include "../args.h"
#include "../tools_call.h"
#include "nlohmann/json.hpp"

static std::string filesystem_tools = R"(
[
    {
        "type": "function",
        "name": "read_file",
        "description": "Read the complete contents of a file from the file system. Handles various text encodings and provides detailed error messages if the file cannot be read. Use this tool when you need to examine the contents of a single file.",
        "parameters": {
            "type": "object",
            "properties": {
                "path": {
                    "type": "string"
                }
            },
            "required": [
                "path"
            ]
        }
    },
    {
        "type": "function",
        "name": "read_multiple_files",
        "description": "Read the contents of multiple files simultaneously. This is more efficient than reading files one by one when you need to analyze or compare multiple files. Each file's content is returned with its path as a reference. Failed reads for individual files won't stop the entire operation.",
        "parameters": {
            "type": "object",
            "properties": {
                "paths": {
                    "type": "array",
                    "items": {
                        "type": "string"
                    }
                }
            },
            "required": [
                "paths"
            ]
        }
    },
    {
        "type": "function",
        "name": "write_file",
        "description": "Create a new file or completely overwrite an existing file with new content. Use with caution as it will overwrite existing files without warning. Handles text content with proper encoding.",
        "parameters": {
            "type": "object",
            "properties": {
                "path": {
                    "type": "string"
                },
                "content": {
                    "type": "string"
                }
            },
            "required": [
                "path",
                "content"
            ]
        }
    },
    {
        "type": "function",
        "name": "edit_file",
        "description": "Request to replace sections of content in an existing file using SEARCH/REPLACE blocks that define exact changes to specific parts of the file. This tool should be used when you need to make targeted changes to specific parts of a file.",
        "parameters": {
            "type": "object",
            "properties": {
                "path": {
                    "type": "string",
                    "description": "The path of the file to modify"
                },
                "diff": {
                    "type": "string",
                    "description": "One or more SEARCH/REPLACE blocks following this exact format:\n```\n<<<<<<< SEARCH\n[exact content to find]\n=======\n[new content to replace with]\n>>>>>>> REPLACE\n```\nCritical rules:\n1. SEARCH content must match the associated file section to find EXACTLY:\n * Match character-for-character including whitespace, indentation, line endings\n * Include all comments, docstrings, etc.\n 2. SEARCH/REPLACE blocks will ONLY replace the first match occurrence.\n * Including multiple unique SEARCH/REPLACE blocks if you need to make multiple changes.\n * Include *just* enough lines in each SEARCH section to uniquely match each set of lines that need to change.\n * When using multiple SEARCH/REPLACE blocks, list them in the order they appear in the file.\n 3. Keep SEARCH/REPLACE blocks concise:\n * Break large SEARCH/REPLACE blocks into a series of smaller blocks that each change a small portion of the file.\n * Include just the changing lines, and a few surrounding lines if needed for uniqueness.\n * Do not include long runs of unchanging lines in SEARCH/REPLACE blocks.\n * Each line must be complete. Never truncate lines mid-way through as this can cause matching failures.\n 4. Special operations:\n * To move code: Use two SEARCH/REPLACE blocks (one to delete from original + one to insert at new location)\n * To delete code: Use empty REPLACE section\n"
                }
            }
        }
    }
]
)";

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
    }
    return std::nullopt;
}

std::optional<std::string> edit_file(nlohmann::json const& args) {
    if (AiArgs::instance().debug) {
        std::cout << "call write_file(" << args.dump() << ")\n";
    }
    if (args.is_object() && args.contains("path") && args["path"].is_string() &&
        args.contains("diff") && args["diff"].is_string()) {
        std::string path = args["path"].get<std::string>();
        std::string diff = args["diff"].get<std::string>();
        std::ifstream in(path);
        std::string file_content{std::istreambuf_iterator<char>(in),
                                 std::istreambuf_iterator<char>()};
        in.close();
        std::string_view search_lable{"<<<<<<< SEARCH"};
        std::string_view replace_lable{">>>>>>> REPLACE"};
        auto search_lable_pos = diff.find(search_lable);
        std::vector<std::string_view> split_lables{"\n=======\n", "=======\n",
                                                   "\n=======", "======="};
        while (search_lable_pos != std::string::npos) {
            auto it =
                std::find_if(begin(split_lables), end(split_lables),
                             [&diff, search_lable_pos](std::string_view lable) {
                                 return diff.find(lable, search_lable_pos) !=
                                        std::string::npos;
                             });
            if (it == end(split_lables)) {
                break;
            }
            std::string_view split_lable{*it};
            auto split_lable_pos = diff.find(split_lable, search_lable_pos);
            auto replace_lable_pos = diff.find(replace_lable, split_lable_pos);
            if (replace_lable_pos == std::string::npos) {
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
            std::ofstream out(path);
            if (out.is_open()) {
                out.write(file_content.data(), file_content.size());
                out.flush();
            }
            search_lable_pos = diff.find(search_lable, replace_lable_pos);
        }
        return "edit_file 已经成功修改文件 " + path;
    }
    return std::nullopt;
}

const std::string& get_filesystem_tools() { return filesystem_tools; }

void regist_filesystem_tools() {
    regist_tools_call("read_file", read_file);
    regist_tools_call("read_multiple_files", read_multiple_files);
    regist_tools_call("write_file", write_file);
    regist_tools_call("edit_file", edit_file);
}
