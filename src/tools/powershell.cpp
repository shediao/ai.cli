#include <chrono>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <subprocess/subprocess.hpp>

#include "ai/function.h"
#include "base/terminal.h"

namespace ai {

namespace {
std::string powershell(nlohmann::json const& args) {
  if (!args.is_object()) {
    return "function powershell arguments is invalid: expected a JSON object.";
  }
  if (!args.contains("command")) {
    return "function powershell arguments is invalid: missing required "
           "parameter \"command\".";
  }
  if (!args["command"].is_string()) {
    return "function powershell arguments is invalid: \"command\" must be a "
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
      "powershell",
      {{"command", command},
       {"working_directory", working_directory},
       {"timeout", timeout_val == $timeout_infinite
                       ? "infinite"
                       : std::to_string(timeout_val)},
       {"requires_confirmation", requires_confirmation ? "true" : "false"}});

  // Check if user confirmation is required
  if (requires_confirmation) {
    ai::base::Terminal tty;
    auto confirmed = tty.confirm("PowerShell command requires confirmation\n" +
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
  auto ret = subprocess::run("powershell", "-NoProfile", "-Command", command,
                             $stdout > out_buf, $stderr > err_buf,
                             $timeout = timeout_val, $cwd = working_directory);
  auto elapsed = std::chrono::steady_clock::now() - start;

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

class PowershellFunction : public ai::Function {
 public:
  std::string call(nlohmann::json const& args) override {
    return powershell(args);
  }
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
  std::string category_ = "powershell";
  nlohmann::json schema_ = R"(
{
  "type": "function",
  "name": "powershell",
  "description": "Execute a PowerShell command and return the output. This tool allows running arbitrary PowerShell commands. The command is executed via 'powershell -NoProfile -Command', so all PowerShell features like cmdlets, pipelines, object manipulation, scripting constructs, and modules are available. Use this tool for advanced Windows system administration, scripting, JSON/XML processing, and any PowerShell tasks. Returns both stdout and stderr output.",
  "parameters": {
    "type": "object",
    "properties": {
      "command": {
        "type": "string",
        "description": "The PowerShell command to execute. This will be passed to 'powershell -NoProfile -Command'. Can include pipelines, cmdlets, variables with $var, and any valid PowerShell syntax. Examples: 'Get-Process | Where-Object {$_.CPU -gt 10}', 'Get-ChildItem -Recurse | Measure-Object', 'Test-Path C:\\Windows'."
      },
      "requires_confirmation": {
        "type": "boolean",
        "description": "Set to true if the command is potentially dangerous or destructive and should require user confirmation before execution. Commands that modify files (Remove-Item, Move-Item), change system settings (Set-ExecutionPolicy, Set-Service), install packages, make network requests, or any other sensitive operations should have this set to true. Set to false for safe read-only operations like Get-ChildItem, Get-Process, Test-Path, etc. Also set to false when the user has explicitly and directly requested the command to be executed - direct user instructions do not require additional confirmation."
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

AUTO_REGISTER(PowershellFunction);

}  // namespace ai
