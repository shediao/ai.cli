#include <chrono>
#include <iostream>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <subprocess/subprocess.hpp>
#include <vector>

#include "ai/function.h"
#include "base/terminal.h"
#include "tools/execute.h"
#include "tools/filesystem.h"

namespace ai {

namespace {
std::string execute_command(nlohmann::json const& args) {
  if (!args.is_object()) {
    return "function execute_command arguments is invalid: expected a JSON "
           "object.";
  }
  auto path_opt = resolve_path(args);

  if (!path_opt.has_value()) {
    if (args.contains("command") && args["command"].is_string()) {
      path_opt = args["command"].get<std::string>();
    }
  }
  if (!path_opt.has_value()) {
    return "function execute_command arguments is invalid: missing required "
           "parameter \"path\".";
  }
  if (path_opt->empty()) {
    return "function execute_command arguments is invalid: \"path\" must be a "
           "string.";
  }

  std::string path = std::move(*path_opt);

  // If path contains a path separator, treat it as a file path and expand
  // tilde. Otherwise treat it as a command name to be found in PATH.
  if (path.find('/') != std::string::npos ||
      path.find('\\') != std::string::npos) {
    path = expand_tilde(path);
  }

  // Build the command vector
  std::vector<std::string> cmd_args;
  if (args.contains("args") && args["args"].is_array()) {
    for (auto const& a : args["args"]) {
      if (a.is_string()) {
        cmd_args.push_back(a.get<std::string>());
      }
    }
  }

  // Execute and capture output
  using namespace subprocess::named_arguments;
  auto timeout_val =
      args.contains("timeout") && args["timeout"].is_number_integer()
          ? args["timeout"].get<int>()
          : timeout_infinite;
  std::string working_directory =
      args.contains("working_directory") &&
              args["working_directory"].is_string()
          ? args["working_directory"].get<std::string>()
          : "";

  std::string args_str;
  for (size_t i = 0; i < cmd_args.size(); ++i) {
    if (i > 0) {
      args_str += " ";
    }
    args_str += cmd_args[i];
  }
  auto requires_confirmation = args.contains("requires_confirmation") &&
                               args["requires_confirmation"].is_boolean() &&
                               args["requires_confirmation"].get<bool>();

  print_toolcall_log(
      "execute_command",
      {{"path", path},
       {"working_directory",
        working_directory.empty() ? "None" : working_directory},
       {"timeout", timeout_val == timeout_infinite
                       ? "infinite"
                       : std::to_string(timeout_val)},
       {"args", args_str},
       {"requires_confirmation", requires_confirmation ? "true" : "false"},
       {"filter", args.contains("filter") ? args["filter"].dump() : "None"}});

  // Check if user confirmation is required
  if (requires_confirmation) {
    ai::base::Terminal tty;
    auto confirmed = tty.confirm("Execute command requires confirmation\n" +
                                 path + " " + args_str + "\nExecute?");
    if (!confirmed) {
      return "Command Execute cancelled by user.";
    }
  }

  subprocess::buffer out_buf{[](const unsigned char* data, size_t size) {
    std::cout.write(reinterpret_cast<const char*>(data), size);
  }};
  subprocess::buffer err_buf{[](const unsigned char* data, size_t size) {
    std::cerr.write(reinterpret_cast<const char*>(data), size);
  }};

  auto start = std::chrono::steady_clock::now();
  auto exit_code = subprocess::run(
      path, cmd_args, $stdin<$devnull, $stdout> out_buf, $stderr > err_buf,
      $timeout = timeout_val, $cwd = working_directory, $newgroup = true);
  auto elapsed = std::chrono::steady_clock::now() - start;

  std::string result;
  result += "Exit code: " + std::to_string(exit_code) + "\n";

  std::string out_str = out_buf.to_string();
  std::string err_str = err_buf.to_string();

  // Apply output filters if specified
  if (args.contains("filter") && args["filter"].is_array()) {
    out_str = filter_lines(out_str, args["filter"]);
    err_str = filter_lines(err_str, args["filter"]);
  }

  if (!out_str.empty()) {
    result += "stdout:\n" + out_str;
    if (!err_str.empty()) {
      result += "\n";
    }
  }
  if (!err_str.empty()) {
    result += "stderr:\n" + err_str;
  }

  if (exit_code != 0 && timeout_val != timeout_infinite &&
      elapsed >= std::chrono::seconds(timeout_val)) {
    result +=
        ("\n\n\nError: command timed out after " + std::to_string(timeout_val) +
         " seconds. Exit code: " + std::to_string(exit_code));
  } else {
    if (out_str.empty() && err_str.empty()) {
      result += "(no output)";
    }
  }

  return result;
}
}  // namespace

class ExecuteCommandFunction : public ai::Function {
 public:
  ExecuteCommandFunction() { add_filter_parameter(schema_); }
  std::string call(nlohmann::json const& args) override {
    return execute_command(args);
  }
  std::string const& category() const override { return category_; }
  nlohmann::json const& schema() const override { return schema_; }

 private:
  std::string category_ = "execute";
  nlohmann::json schema_ = R"===(
{
  "type": "function",
  "name": "execute_command",
  "description": "Execute a command as a subprocess and capture its output. Returns the exit code, stdout, and stderr. Use this tool when you need to run a command. NOTE: This is NOT a shell — the command is executed directly via exec/CreateProcessW using path + args as argv. No shell parsing, glob expansion, pipes, redirects, or variable substitution will occur. If you need shell features, use the bash, cmd, or powershell tools instead. If the path contains a path separator (e.g. '/', '\\'), it is treated as a file path to an executable; otherwise it is treated as a command name resolved from PATH.",
  "parameters": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string",
        "description": "The command to run. This is executed directly (not via a shell) — the path and args form the argv array passed to exec/CreateProcessW. If it contains a path separator, it is treated as a file path to an executable. Otherwise it is treated as a command name resolved from PATH."
      },
      "args": {
        "type": "array",
        "items": {
          "type": "string"
        },
        "description": "Optional arguments to pass to the command."
      },
      "requires_confirmation": {
        "type": "boolean",
        "description": "Set to true if the command is potentially dangerous or destructive and should require user confirmation before execution. Commands that modify files (rm, mv, dd), change system settings, install packages, use sudo, make network requests that could expose data, or any other sensitive operations should have this set to true. Set to false for safe read-only operations like cat, ls, find, grep, etc. Also set to false when the user has explicitly and directly requested the command to be executed - direct user instructions do not require additional confirmation."
      },
      "timeout": {
        "type": "integer",
        "description": "Optional timeout in seconds. If the command does not complete within this time, it will be terminated. If not provided, no timeout is applied."
      },
      "working_directory": {
        "type": "string",
        "description": "Optional working directory for the command. If provided, the command will be run in this directory. Can be an absolute path or a relative path (resolved against the current working directory)."
      }
    },
    "required": ["path"]
  }
}
)==="_json;
};

AUTO_REGISTER(ExecuteCommandFunction);

}  // namespace ai
