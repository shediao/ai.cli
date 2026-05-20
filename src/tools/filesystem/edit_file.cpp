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

extern std::string expand_tilde(std::string const& path);
extern std::optional<std::string> resolve_path(nlohmann::json const& args);

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
  auto file_content_opt = ai::utils::read_file(path);
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
    ai::utils::TempFile temp("",
                             std::filesystem::path(path).filename().string());
    ai::utils::write_file(temp.path(), file_content);
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
  if (!ai::utils::write_file(path, file_content)) {
    return "Failed to write to file: " + path;
  }

  return "Successfully edited file " + path;
}
