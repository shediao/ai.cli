#include <chrono>
#include <environment/environment.hpp>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <subprocess/subprocess.hpp>

#include "ai/function.h"
#include "base/terminal.h"
#include "tools/execute.h"

namespace ai {

namespace {
std::string bash(nlohmann::json const& args) {
  if (!args.is_object()) {
    return "function bash arguments is invalid: expected a JSON object.";
  }
  if (!args.contains("command")) {
    return "function bash arguments is invalid: missing required parameter "
           "\"command\".";
  }
  if (!args["command"].is_string()) {
    return "function bash arguments is invalid: \"command\" must be a "
           "string.";
  }
  std::string command = args["command"].get<std::string>();

  auto requires_confirmation = args.contains("requires_confirmation") &&
                               args["requires_confirmation"].is_boolean() &&
                               args["requires_confirmation"].get<bool>();
  auto timeout_val =
      args.contains("timeout") && args["timeout"].is_number_integer()
          ? args["timeout"].get<int>()
          : $timeout_infinite;
  std::string working_directory =
      args.contains("working_directory") &&
              args["working_directory"].is_string()
          ? args["working_directory"].get<std::string>()
          : "";

  print_toolcall_log(
      "bash",
      {{"command", command},
       {"working_directory",
        working_directory.empty() ? "None" : working_directory},
       {"timeout", timeout_val == $timeout_infinite
                       ? "infinite"
                       : std::to_string(timeout_val)},
       {"requires_confirmation", requires_confirmation ? "true" : "false"},
       {"filter", args.contains("filter") ? args["filter"].dump() : "None"}});

  // Check if user confirmation is required
  if (requires_confirmation) {
    ai::base::Terminal tty;
    auto confirmed = tty.confirm("Bash command requires confirmation:\n" +
                                 command + "\nExecute?");
    if (!confirmed) {
      return "Command cancelled by user: " + command;
    }
  }
  subprocess::buffer out_buf{[](const unsigned char* data, size_t size) {
    std::cout.write(reinterpret_cast<const char*>(data), size);
  }};
  subprocess::buffer err_buf{[](const unsigned char* data, size_t size) {
    std::cerr.write(reinterpret_cast<const char*>(data), size);
  }};

  auto start = std::chrono::steady_clock::now();
  using subprocess::named_arguments::bash;
  auto ret = subprocess::run(bash, command, $stdin<$devnull, $stdout> out_buf,
                             $stderr > err_buf, $timeout = timeout_val,
                             $cwd = working_directory, $newgroup = true);
  auto elapsed = std::chrono::steady_clock::now() - start;

  std::string out_str = out_buf.to_string();
  std::string err_str = err_buf.to_string();

  // Apply output filters if specified
  if (args.contains("filter") && args["filter"].is_array()) {
    out_str = filter_lines(out_str, args["filter"]);
    err_str = filter_lines(err_str, args["filter"]);
  }
  std::string result;
  if (!out_str.empty()) {
    result += out_str;
  }
  if (!err_str.empty()) {
    if (!result.empty() && result.back() != '\n') {
      result += '\n';
    }
    result += err_str;
  }

  if (ret != 0 && timeout_val != $timeout_infinite &&
      elapsed >= std::chrono::seconds(timeout_val)) {
    result +=
        ("\n\n\nError: command timed out after " + std::to_string(timeout_val) +
         " seconds. Exit code: " + std::to_string(ret));
  } else {
    if (result.empty()) {
      result = "Command executed successfully with no output. Exit code: " +
               std::to_string(ret);
    }
  }
  return result;
}
}  // namespace

class BashFunction : public ai::Function {
 public:
  BashFunction() { add_filter_parameter(schema_); }
  std::string call(nlohmann::json const& args) override { return bash(args); }
  bool enabled() const override {
#if !defined(_WIN32)
    return true;
#else
    // On Windows, only enable if bash is available (via SHELL env or PATH).
    if (auto shell = env::get("SHELL"); shell.has_value()) {
      auto const& s = shell.value();
      if (s.ends_with("bash") || s.ends_with("bash.exe")) {
        return true;
      }
    }
    for (auto const& path : env::path()) {
      if (std::filesystem::exists(std::filesystem::path(path) / "bash.exe")) {
        return true;
      }
    }
    return false;
#endif
  }
  std::string const& category() const override { return category_; }
  nlohmann::json const& schema() const override { return schema_; }

 private:
  std::string category_ = "execute";
  nlohmann::json schema_ = R"(
{
  "type": "function",
  "name": "bash",
  "description": "Execute a bash command and return the output. This tool allows running arbitrary shell commands. The command is executed via 'bash -c', so all bash features like pipes, redirects, variables, and command substitution are available. Use this tool for file operations, system queries, building projects, running scripts, and any other shell-level tasks. Returns both stdout and stderr output.",
  "parameters": {
    "type": "object",
    "properties": {
      "command": {
        "type": "string",
        "description": "The bash command to execute. This will be passed to 'bash -c'. Can include pipes, redirects, variable expansions, and any valid bash syntax. To reduce log noise, prefer piping through 'tail -n N' to limit output lines or 'grep xxx' to filter for relevant content, especially when running commands that produce verbose output (e.g., builds, logs, large listings)."
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
        "description": "Optional working directory for the command. If provided, the command will be executed in this directory. Can be an absolute path or a relative path (resolved against the current working directory)."
      }
    },
    "required": ["command"]
  }
}
)"_json;
};

AUTO_REGISTER(BashFunction);

}  // namespace ai
