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
  if (!args["diff"].is_array()) {
    return "function edit_file arguments is invalid: \"diff\" must be an "
           "array.";
  }

  std::string path = std::move(*path_opt);
  path = expand_tilde(path);
  auto const& diff_array = args["diff"];

  std::vector<std::pair<std::string, std::string>> log_args{{"path", path}};
  for (auto const& item : diff_array) {
    if (item.is_object()) {
      if (item.contains("search") && item["search"].is_string()) {
        log_args.emplace_back("search",
                              "\n" + item["search"].get<std::string>());
      }
      if (item.contains("replace") && item["replace"].is_string()) {
        log_args.emplace_back("replace",
                              "\n" + item["replace"].get<std::string>());
      }
    }
  }
  print_toolcall_log("edit_file", log_args);

  // --- read original file ---
  auto file_content_opt = ai::base::read_file(path);
  if (!file_content_opt.has_value()) {
    return "Failed to open file: " + path;
  }
  std::string file_content = std::move(file_content_opt.value());

  // --- validate and apply each diff item ---
  for (size_t i = 0; i < diff_array.size(); i++) {
    auto const& item = diff_array[i];
    if (!item.is_object()) {
      return "Failed to edit file " + path + ": diff item " +
             std::to_string(i + 1) +
             " must be an object with \"search\" and \"replace\" fields.";
    }
    if (!item.contains("search") || !item["search"].is_string()) {
      return "Failed to edit file " + path + ": diff item " +
             std::to_string(i + 1) +
             " is missing required \"search\" string field.";
    }
    if (!item.contains("replace") || !item["replace"].is_string()) {
      return "Failed to edit file " + path + ": diff item " +
             std::to_string(i + 1) +
             " is missing required \"replace\" string field.";
    }

    std::string search = item["search"].get<std::string>();
    std::string replace = item["replace"].get<std::string>();

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
  "description": "Request to replace sections of content in an existing file using SEARCH/REPLACE objects that define exact changes to specific parts of the file. This tool should be used when you need to make targeted changes to specific parts of a file. If the edit fails due to matching issues, fall back to using the write_file tool to rewrite the entire file with the corrected content.",
  "parameters": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string",
        "description": "The path of the file to modify"
      },
      "diff": {
        "type": "array",
        "description": "An array of SEARCH/REPLACE objects. Each object specifies an exact change to a specific part of the file. Use multiple objects when you need to make multiple changes.\n\nArray item format:\n{\n  \"search\": \"[exact content to find]\",\n  \"replace\": \"[new content to replace with]\"\n}\n\nCritical rules:\n1. SEARCH content must match the associated file section to find EXACTLY:\n * Match character-for-character including whitespace, indentation, line endings\n * Include all comments, docstrings, etc.\n2. Each SEARCH/REPLACE object will ONLY replace the first match occurrence.\n * Use multiple array items if you need to make multiple changes.\n * Include *just* enough lines in each SEARCH section to uniquely match each set of lines that need to change.\n * When using multiple items, list them in the order they appear in the file.\n3. Keep SEARCH/REPLACE blocks concise:\n * Break large changes into a series of smaller items that each change a small portion of the file.\n * Include just the changing lines, and a few surrounding lines if needed for uniqueness.\n * Do not include long runs of unchanging lines in SEARCH/REPLACE blocks.\n * Each line must be complete. Never truncate lines mid-way through as this can cause matching failures.\n4. Special operations:\n * To move code: Use two items (one to delete from original + one to insert at new location)\n * To delete code: Use empty replace string",
        "items": {
          "type": "object",
          "properties": {
            "search": {
              "type": "string",
              "description": "The exact content to find (case-sensitive). Must match character-for-character including whitespace and indentation."
            },
            "replace": {
              "type": "string",
              "description": "The new content to replace with. Use an empty string to delete the matched content."
            }
          },
          "required": ["search", "replace"]
        }
      }
    },
    "required": ["path", "diff"]
  }
}
)==="_json;
};

AUTO_REGISTER(EditFileFunction);

}  // namespace ai
