#include <chrono>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <subprocess/subprocess.hpp>

#include "ai/function.h"
#include "ai/utils.h"
#include "powershell_tools_json.h"

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
    ai::utils::Terminal tty;
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

std::string_view get_powershell_tools() { return powershell_tools_json_str; }

void regist_powershell_tools() { regist_tool_calls("powershell", powershell); }

// Self-register the category at static-init time
static bool _powershell_tool_category_registered = regist_tool_category(
    "powershell", get_powershell_tools, regist_powershell_tools);
