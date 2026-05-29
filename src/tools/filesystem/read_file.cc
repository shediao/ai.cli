#include <algorithm>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ai/function.h"
#include "base/file.h"

namespace ai {

extern std::string expand_tilde(std::string const& path);
extern std::optional<std::string> resolve_path(nlohmann::json const& args);

namespace {
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

  auto content_opt = ai::base::read_file(path);
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
    }
    if (limit > 0) {
      auto end = it;
      for (int i = 0; i < limit; ++i) {
        end = std::find(end, content.end(), '\n');
        if (end == content.end()) {
          break;
        }
        ++end;
      }
      return std::string(it, end);
    }
    return std::string(it, content.end());
  }

  return content;
}
}  // namespace

class ReadFileFunction : public ai::Function {
 public:
  std::string call(nlohmann::json const& args) override {
    return read_file(args);
  }
  std::string const& category() const override { return category_; }
  nlohmann::json const& schema() const override { return schema_; }

 private:
  std::string category_ = "filesystem";
  nlohmann::json schema_ = R"===(
{
  "type": "function",
  "name": "read_file",
  "description": "Read the complete contents of a file from the file system. Handles various text encodings and provides detailed error messages if the file cannot be read. Use this tool when you need to examine the contents of a single file. Use 'offset' and 'limit' parameters together to read long files in chunks, especially handy for long files, but it's recommended to read the whole file by not providing these parameters.",
  "parameters": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string"
      },
      "offset": {
        "type": "integer",
        "description": "The line number to start reading from. Only provide if the file is too large to read at once. Line numbers are 1-indexed (the first line is line 1)."
      },
      "limit": {
        "type": "integer",
        "description": "The number of lines to read. Only provide if the file is too large to read at once."
      }
    },
    "required": ["path"]
  }
}
)==="_json;
};

AUTO_REGISTER(ReadFileFunction);

}  // namespace ai
