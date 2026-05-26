#include <algorithm>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <subprocess/subprocess.hpp>

#include "ai/function.h"
#include "base/file.h"
#include "base/temp_file.h"

namespace ai {

extern std::string expand_tilde(std::string const& path);
extern std::optional<std::string> resolve_path(nlohmann::json const& args);
extern std::string append_prefix_per_line(std::string_view str,
                                          std::string_view prefix);

namespace {
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
  auto file_content_opt = ai::base::read_file(path);
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
    ai::base::TempFile temp("",
                            std::filesystem::path(path).filename().string());
    ai::base::write_file(temp.path(), new_content);
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
  if (!ai::base::write_file(path, new_content)) {
    return "Failed to write to file: " + path;
  }

  return "Successfully replaced lines " + std::to_string(start_line) + "-" +
         std::to_string(end_line) + " in " + path;
}
}  // namespace

class ReplaceLinesFunction : public ai::Function {
 public:
  std::string call(nlohmann::json const& args) override {
    return replace_lines(args);
  }
  std::string const& category() const override { return category_; }
  nlohmann::json const& schema() const override { return schema_; }

 private:
  std::string category_ = "filesystem";
  nlohmann::json schema_ = R"===(
{
  "type": "function",
  "name": "replace_lines",
  "description": "Replace a range of lines in a file with new content. Uses 1-indexed line numbers. The range [start_line, end_line] is inclusive on both ends. The specified lines are removed and replaced with the provided content string. Use this tool for precise line-range based edits without needing to match exact text.",
  "parameters": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string",
        "description": "The path of the file to modify."
      },
      "start_line": {
        "type": "integer",
        "description": "The 1-indexed line number to start replacing from (inclusive). Must be >= 1."
      },
      "end_line": {
        "type": "integer",
        "description": "The 1-indexed line number to stop replacing at (inclusive). Must be >= start_line."
      },
      "content": {
        "type": "string",
        "description": "The new content to insert in place of the specified line range. Can be an empty string to delete lines, a single line, or multiple lines."
      }
    },
    "required": ["path", "start_line", "end_line", "content"]
  }
}
)==="_json;
};

AUTO_REGISTER(ReplaceLinesFunction);

}  // namespace ai
