#include <algorithm>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <subprocess/subprocess.hpp>
#include <vector>

#include "ai/function.h"
#include "ai/utils.h"
#include "base/file.h"
#include "base/temp_file.h"

namespace ai {

extern std::string expand_tilde(std::string const& path);
extern std::optional<std::string> resolve_path(nlohmann::json const& args);

namespace {
std::string edit_file(nlohmann::json const& args) {
  // --- argument validation ---
  if (!args.is_object()) {
    return "function edit_file arguments is invalid: expected a JSON object.";
  }
  auto path_opt = resolve_path(args);
  if (!path_opt.has_value()) {
    return "function edit_file arguments is invalid: missing required "
           "parameter \"path\".";
  }
  if (path_opt->empty()) {
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

  std::string path = std::move(*path_opt);
  path = expand_tilde(path);
  std::string diff = args["diff"].get<std::string>();

  print_toolcall_log("edit_file", {{"path", path}, {"diff", diff}});

  // --- read original file ---
  auto file_content_opt = ai::base::read_file(path);
  if (!file_content_opt.has_value()) {
    return "Failed to open file: " + path;
  }
  std::string file_content = std::move(file_content_opt.value());

#define SEARCH_LABLE "<<<<<<< SEARCH"
#define SEPARATOR_LABLE "======="
#define REPLACE_LABLE ">>>>>>> REPLACE"

  // 1. 确认是否以 '<<<<<<< SEARCH' 开头
  if (!diff.starts_with(SEARCH_LABLE "\n")) {
    return "Failed to edit file " + path +
           ": diff must start with \"<<<<<<< SEARCH\" followed by a "
           "newline.";
  }
  // 2. 是否以 '

  std::vector<std::string::size_type> search_indexes;     // <<<<<<< SEARCH
  std::vector<std::string::size_type> separator_indexes;  // =======
  std::vector<std::string::size_type> replace_indexes;    // >>>>>>> REPLACE

  auto it = diff.find(SEARCH_LABLE "\n", 0);
  while (it != std::string::npos) {
    search_indexes.push_back(it);
    it = diff.find(SEARCH_LABLE "\n", it + 15);
  }

  it = diff.find("\n" SEPARATOR_LABLE "\n", 0);
  while (it != std::string::npos) {
    separator_indexes.push_back(it + 1);
    it = diff.find("\n" SEPARATOR_LABLE "\n", it + 9);
  }

  it = diff.find("\n" REPLACE_LABLE, 0);
  while (it != std::string::npos) {
    replace_indexes.push_back(it + 1);
    it = diff.find("\n" REPLACE_LABLE, it + 16);
  }

  it = diff.find("\n" SEPARATOR_LABLE REPLACE_LABLE, 0);
  while (it != std::string::npos) {
    separator_indexes.push_back(it + 1);
    replace_indexes.push_back(it + 1 + 7);
    it = diff.find("\n" SEPARATOR_LABLE REPLACE_LABLE, it + 23);
  }

  // 3. 三个标签的个数应该一样
  if (search_indexes.size() != separator_indexes.size() ||
      separator_indexes.size() != replace_indexes.size()) {
    return "Failed to edit file " + path +
           ": mismatched number of SEARCH/SEPARATOR/REPLACE labels in "
           "diff. Found " +
           std::to_string(search_indexes.size()) + " SEARCH, " +
           std::to_string(separator_indexes.size()) + " separator, " +
           std::to_string(replace_indexes.size()) + " REPLACE labels.";
  }

  // 4. 检测每个标签组是否匹配
  for (size_t i = 0; i < search_indexes.size(); i++) {
    if (search_indexes[i] >= separator_indexes[i] ||
        separator_indexes[i] >= replace_indexes[i]) {
      return "Failed to edit file " + path +
             ": SEARCH/SEPARATOR/REPLACE labels are out of order in "
             "diff block " +
             std::to_string(i + 1) + ".";
    }
  }

  for (size_t i = 0; i < search_indexes.size(); i++) {
    std::string_view search{diff.data() + search_indexes[i] + 15,
                            separator_indexes[i] - search_indexes[i] - 15};
    std::string_view replace{diff.data() + separator_indexes[i] + 8,
                             replace_indexes[i] - separator_indexes[i] - 8};
    auto search_it = file_content.find(search);
    if (search_it != std::string::npos) {
      file_content.replace(search_it, search.size(), replace);
    } else {
      return "Failed to edit file " + path + ": SEARCH block " +
             std::to_string(i + 1) +
             " was not found in the file. The content to replace may "
             "have already been modified or does not exactly match.";
    }
  }

  // --- show diff between original file and modified content ---
  {
    ai::base::TempFile temp("",
                            std::filesystem::path(path).filename().string());
    ai::base::write_file(temp.path(), file_content);
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
  if (!ai::base::write_file(path, file_content)) {
    return "Failed to write to file: " + path;
  }

  return "Successfully edited file " + path;
}
}  // namespace

class EditFileFunction : public ai::Function {
 public:
  std::string call(nlohmann::json const& args) override {
    return edit_file(args);
  }
  std::string const& category() const override { return category_; }
  nlohmann::json const& schema() const override { return schema_; }

 private:
  std::string category_ = "filesystem";
  nlohmann::json schema_ = R"===(
{
  "type": "function",
  "name": "edit_file",
  "description": "Request to replace sections of content in an existing file using SEARCH/REPLACE blocks that define exact changes to specific parts of the file. This tool should be used when you need to make targeted changes to specific parts of a file. If the edit fails due to matching issues, fall back to using the write_file tool to rewrite the entire file with the corrected content.",
  "parameters": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string",
        "description": "The path of the file to modify"
      },
      "diff": {
        "type": "string",
        "description": "One or more SEARCH/REPLACE blocks following this exact format:\n```\n<<<<<<< SEARCH\n[exact content to find]\n=======\n[new content to replace with]\n>>>>>>> REPLACE\n```\nCritical rules:\n1. SEARCH content must match the associated file section to find EXACTLY:\n * Match character-for-character including whitespace, indentation, line endings\n * Include all comments, docstrings, etc.\n 2. SEARCH/REPLACE blocks will ONLY replace the first match occurrence.\n * Including multiple unique SEARCH/REPLACE blocks if you need to make multiple changes.\n * Include *just* enough lines in each SEARCH section to uniquely match each set of lines that need to change.\n * When using multiple SEARCH/REPLACE blocks, list them in the order they appear in the file.\n 3. Keep SEARCH/REPLACE blocks concise:\n * Break large SEARCH/REPLACE blocks into a series of smaller blocks that each change a small portion of the file.\n * Include just the changing lines, and a few surrounding lines if needed for uniqueness.\n * Do not include long runs of unchanging lines in SEARCH/REPLACE blocks.\n * Each line must be complete. Never truncate lines mid-way through as this can cause matching failures.\n 4. Special operations:\n * To move code: Use two SEARCH/REPLACE blocks (one to delete from original + one to insert at new location)\n * To delete code: Use empty REPLACE section\n5. Delimiters '<<<<<<< SEARCH', '=======', '>>>>>>> REPLACE' need to be on separate lines"
      }
    },
    "required": ["path", "diff"]
  }
}
)==="_json;
};

AUTO_REGISTER(EditFileFunction);

}  // namespace ai
