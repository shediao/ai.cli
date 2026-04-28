#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <subprocess/subprocess.hpp>
#include <vector>

#include "git_tools_json.h"
#include "logging.h"
#include "tool_calls.h"

namespace {

/// Helper: run a git command in the given repository path and capture
/// stdout+stderr. Builds a command vector and executes directly via
/// subprocess::run.
std::string run_git(std::string const& repo_path,
                    std::vector<std::string> const& args) {
  subprocess::buffer out_buf;
  subprocess::buffer err_buf;

  using namespace subprocess::named_arguments;
  using subprocess::run;

  // Build the command vector
  std::vector<std::string> cmd{"git"};
  if (!repo_path.empty()) {
    cmd.push_back("-C");
    cmd.push_back(repo_path);
  }
  cmd.insert(cmd.end(), args.begin(), args.end());

  int ret = run(std::move(cmd), std_out > out_buf, std_err > err_buf);

  std::string result;
  if (!out_buf.empty()) {
    result += out_buf.to_string();
  }
  if (!err_buf.empty()) {
    if (!result.empty() && result.back() != '\n') {
      result += '\n';
    }
    result += err_buf.to_string();
  }

  if (result.empty()) {
    result =
        "git command completed with exit code " + std::to_string(ret) + ".";
  }

  return result;
}

/// Helper: extract the optional "path" parameter from args.
std::string get_repo_path(nlohmann::json const& args) {
  if (args.is_object() && args.contains("path") && args["path"].is_string()) {
    return args["path"].get<std::string>();
  }
  return "";
}

}  // namespace

// ── git_status ────────────────────────────────────────────────────────
std::string git_status(nlohmann::json const& args) {
  LOG(INFO) << "call git_status(" << args.dump() << ")";

  if (!args.is_object()) {
    return "function git_status arguments is invalid: expected a JSON "
           "object.";
  }

  std::string repo = get_repo_path(args);
  return run_git(repo, {"status", "--porcelain", "--branch"});
}

// ── git_diff ──────────────────────────────────────────────────────────
std::string git_diff(nlohmann::json const& args) {
  LOG(INFO) << "call git_diff(" << args.dump() << ")";

  if (!args.is_object()) {
    return "function git_diff arguments is invalid: expected a JSON object.";
  }

  std::string repo = get_repo_path(args);
  std::vector<std::string> cmd_args{"diff"};

  // staged changes
  if (args.contains("staged") && args["staged"].is_boolean() &&
      args["staged"].get<bool>()) {
    cmd_args.push_back("--staged");
  }

  // commit range
  if (args.contains("commit1") && args["commit1"].is_string()) {
    std::string c1 = args["commit1"].get<std::string>();
    if (args.contains("commit2") && args["commit2"].is_string()) {
      std::string c2 = args["commit2"].get<std::string>();
      cmd_args.push_back(c1 + ".." + c2);
    } else {
      cmd_args.push_back(c1);
    }
  }

  // file restriction
  if (args.contains("file") && args["file"].is_string()) {
    cmd_args.push_back("--");
    cmd_args.push_back(args["file"].get<std::string>());
  }

  return run_git(repo, cmd_args);
}

// ── git_log ───────────────────────────────────────────────────────────
std::string git_log(nlohmann::json const& args) {
  LOG(INFO) << "call git_log(" << args.dump() << ")";

  if (!args.is_object()) {
    return "function git_log arguments is invalid: expected a JSON object.";
  }

  std::string repo = get_repo_path(args);
  std::vector<std::string> cmd_args{"log"};

  // oneline format
  if (args.contains("oneline") && args["oneline"].is_boolean() &&
      args["oneline"].get<bool>()) {
    cmd_args.push_back("--oneline");
  } else {
    cmd_args.push_back("--format=%h %an %ad %s");  // hash author date subject
  }

  // number of commits
  if (args.contains("count") && args["count"].is_number_integer()) {
    int n = args["count"].get<int>();
    if (n > 0) {
      cmd_args.push_back("-n");
      cmd_args.push_back(std::to_string(n));
    }
  } else {
    cmd_args.push_back("-n");
    cmd_args.push_back("20");  // default
  }

  // branch
  if (args.contains("branch") && args["branch"].is_string()) {
    cmd_args.push_back(args["branch"].get<std::string>());
  }

  // file
  if (args.contains("file") && args["file"].is_string()) {
    cmd_args.push_back("--");
    cmd_args.push_back(args["file"].get<std::string>());
  }

  return run_git(repo, cmd_args);
}

// ── git_add ───────────────────────────────────────────────────────────
std::string git_add(nlohmann::json const& args) {
  LOG(INFO) << "call git_add(" << args.dump() << ")";

  if (!args.is_object()) {
    return "function git_add arguments is invalid: expected a JSON object.";
  }

  if (!args.contains("files")) {
    return "function git_add arguments is invalid: missing required "
           "parameter \"files\".";
  }
  if (!args["files"].is_array()) {
    return "function git_add arguments is invalid: \"files\" must be an "
           "array.";
  }

  std::string repo = get_repo_path(args);
  std::vector<std::string> cmd_args{"add"};

  for (auto const& f : args["files"]) {
    if (f.is_string()) {
      cmd_args.push_back(f.get<std::string>());
    }
  }

  if (cmd_args.size() == 1) {
    return "function git_add arguments is invalid: \"files\" array is empty.";
  }

  return run_git(repo, cmd_args);
}

// ── git_commit ────────────────────────────────────────────────────────
std::string git_commit(nlohmann::json const& args) {
  LOG(INFO) << "call git_commit(" << args.dump() << ")";

  if (!args.is_object()) {
    return "function git_commit arguments is invalid: expected a JSON "
           "object.";
  }

  if (!args.contains("message")) {
    return "function git_commit arguments is invalid: missing required "
           "parameter \"message\".";
  }
  if (!args["message"].is_string()) {
    return "function git_commit arguments is invalid: \"message\" must be a "
           "string.";
  }

  std::string repo = get_repo_path(args);
  std::string msg = args["message"].get<std::string>();

  return run_git(repo, {"commit", "-m", msg});
}

// ── git_branch ────────────────────────────────────────────────────────
std::string git_branch(nlohmann::json const& args) {
  LOG(INFO) << "call git_branch(" << args.dump() << ")";

  if (!args.is_object()) {
    return "function git_branch arguments is invalid: expected a JSON "
           "object.";
  }

  std::string repo = get_repo_path(args);
  std::vector<std::string> cmd_args{"branch"};

  // remote branches
  if (args.contains("remote") && args["remote"].is_boolean() &&
      args["remote"].get<bool>()) {
    cmd_args.push_back("--remote");
  }

  // delete
  if (args.contains("delete") && args["delete"].is_boolean() &&
      args["delete"].get<bool>()) {
    cmd_args.push_back("-d");
  }

  // branch name (for create/delete)
  if (args.contains("name") && args["name"].is_string()) {
    cmd_args.push_back(args["name"].get<std::string>());
  }

  return run_git(repo, cmd_args);
}

// ── git_checkout ──────────────────────────────────────────────────────
std::string git_checkout(nlohmann::json const& args) {
  LOG(INFO) << "call git_checkout(" << args.dump() << ")";

  if (!args.is_object()) {
    return "function git_checkout arguments is invalid: expected a JSON "
           "object.";
  }

  std::string repo = get_repo_path(args);
  std::vector<std::string> cmd_args{"checkout"};

  // create branch and switch
  if (args.contains("create") && args["create"].is_boolean() &&
      args["create"].get<bool>()) {
    cmd_args.push_back("-b");
  }

  // branch or file
  if (args.contains("branch") && args["branch"].is_string()) {
    cmd_args.push_back(args["branch"].get<std::string>());
  } else if (args.contains("file") && args["file"].is_string()) {
    cmd_args.push_back("--");
    cmd_args.push_back(args["file"].get<std::string>());
  } else {
    return "function git_checkout arguments is invalid: missing \"branch\" "
           "or \"file\" parameter.";
  }

  return run_git(repo, cmd_args);
}

// ── git_init ──────────────────────────────────────────────────────────
std::string git_init(nlohmann::json const& args) {
  LOG(INFO) << "call git_init(" << args.dump() << ")";

  if (!args.is_object()) {
    return "function git_init arguments is invalid: expected a JSON object.";
  }

  std::string repo = get_repo_path(args);

  // git init doesn't use -C (the directory may not exist yet), so we build
  // the command vector directly.
  std::vector<std::string> cmd{"git", "init"};
  if (args.contains("bare") && args["bare"].is_boolean() &&
      args["bare"].get<bool>()) {
    cmd.push_back("--bare");
  }
  if (!repo.empty()) {
    cmd.push_back(repo);
  }

  subprocess::buffer out_buf;
  subprocess::buffer err_buf;
  using namespace subprocess::named_arguments;
  using subprocess::run;

  int ret = run(std::move(cmd), std_out > out_buf, std_err > err_buf);

  std::string result;
  if (!out_buf.empty()) {
    result += out_buf.to_string();
  }
  if (!err_buf.empty()) {
    if (!result.empty() && result.back() != '\n') {
      result += '\n';
    }
    result += err_buf.to_string();
  }
  if (result.empty()) {
    result = "git init completed with exit code " + std::to_string(ret) + ".";
  }
  return result;
}

// ── git_clone ─────────────────────────────────────────────────────────
std::string git_clone(nlohmann::json const& args) {
  LOG(INFO) << "call git_clone(" << args.dump() << ")";

  if (!args.is_object()) {
    return "function git_clone arguments is invalid: expected a JSON object.";
  }

  if (!args.contains("url")) {
    return "function git_clone arguments is invalid: missing required "
           "parameter \"url\".";
  }
  if (!args["url"].is_string()) {
    return "function git_clone arguments is invalid: \"url\" must be a "
           "string.";
  }

  std::string url = args["url"].get<std::string>();

  // git clone doesn't use -C, so build the command vector directly.
  std::vector<std::string> cmd{"git", "clone"};

  // depth (shallow clone)
  if (args.contains("depth") && args["depth"].is_number_integer()) {
    int d = args["depth"].get<int>();
    if (d > 0) {
      cmd.push_back("--depth");
      cmd.push_back(std::to_string(d));
    }
  }

  cmd.push_back(url);

  // target directory
  if (args.contains("directory") && args["directory"].is_string()) {
    cmd.push_back(args["directory"].get<std::string>());
  }

  subprocess::buffer out_buf;
  subprocess::buffer err_buf;
  using namespace subprocess::named_arguments;
  using subprocess::run;

  int ret = run(std::move(cmd), std_out > out_buf, std_err > err_buf);

  std::string result;
  if (!out_buf.empty()) {
    result += out_buf.to_string();
  }
  if (!err_buf.empty()) {
    if (!result.empty() && result.back() != '\n') {
      result += '\n';
    }
    result += err_buf.to_string();
  }
  if (result.empty()) {
    result = "git clone completed with exit code " + std::to_string(ret) + ".";
  }
  return result;
}

// ── Category wiring ───────────────────────────────────────────────────

std::string_view get_git_tools() { return git_tools_json_str; }

void regist_git_tools() {
  regist_tool_calls("git_status", git_status);
  regist_tool_calls("git_diff", git_diff);
  regist_tool_calls("git_log", git_log);
  regist_tool_calls("git_add", git_add);
  regist_tool_calls("git_commit", git_commit);
  regist_tool_calls("git_branch", git_branch);
  regist_tool_calls("git_checkout", git_checkout);
  regist_tool_calls("git_init", git_init);
  regist_tool_calls("git_clone", git_clone);
}

// Self-register the category at static-init time
static bool _git_tool_category_registered =
    regist_tool_category("git", get_git_tools, regist_git_tools);
