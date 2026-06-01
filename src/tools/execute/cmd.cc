#include <chrono>
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
std::string cmd(nlohmann::json const& args) {
  if (!args.is_object()) {
    return "function cmd arguments is invalid: expected a JSON object.";
  }
  if (!args.contains("command")) {
    return "function cmd arguments is invalid: missing required parameter "
           "\"command\".";
  }
  if (!args["command"].is_string()) {
    return "function cmd arguments is invalid: \"command\" must be a "
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
      "cmd",
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
    auto confirmed = tty.confirm("CMD command requires confirmation\n" +
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
#if defined(_WIN32)
  using subprocess::named_arguments::shell;
  auto ret = subprocess::run(shell, command, $stdin<$devnull, $stdout> out_buf,
                             $stderr > err_buf, $timeout = timeout_val,
                             $cwd = working_directory, $newgroup = true);
#else
  auto ret = subprocess::run("cmd", "/d", "/s", "/c", command,
                             $stdin<$devnull, $stdout> out_buf,
                             $stderr > err_buf, $timeout = timeout_val,
                             $cwd = working_directory, $newgroup = true);
#endif
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

class CmdFunction : public ai::Function {
 public:
  CmdFunction() { add_filter_parameter(schema_); }
  std::string call(nlohmann::json const& args) override { return cmd(args); }
  bool enabled() const override {
#if defined(_WIN32)
    return true;
#else
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
  "name": "cmd",
  "description": "Execute a Windows CMD command and return the output. This tool allows running arbitrary Windows Command Prompt commands. The command is executed via 'cmd /d /s /c', so all cmd features like pipelines, redirects, variable expansions, and built-in commands (dir, type, echo, set, etc.) are available. Use this tool for Windows system administration, file operations, process management, and any Command Prompt tasks. Returns both stdout and stderr output.",
  "parameters": {
    "type": "object",
    "properties": {
      "command": {
        "type": "string",
        "description": "The CMD command to execute. This will be passed to 'cmd /d /s /c'. Can include pipelines, redirects, variable expansions with %VAR%, and any valid CMD syntax. Examples: 'dir /b', 'type file.txt', 'echo %PATH%', 'tasklist | findstr chrome'."
      },
      "requires_confirmation": {
        "type": "boolean",
        "description": "Set to true if the command is potentially dangerous or destructive and should require user confirmation before execution. Commands that modify files (del, move, ren), change system settings (reg, netsh), install packages, make network requests, or any other sensitive operations should have this set to true. Set to false for safe read-only operations like dir, type, echo, etc. Also set to false when the user has explicitly and directly requested the command to be executed - direct user instructions do not require additional confirmation."
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

AUTO_REGISTER(CmdFunction);

}  // namespace ai
