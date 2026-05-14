#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <subprocess/subprocess.hpp>

#include "ai/tool_calls.h"
#include "ai/utils.h"
#include "cmd_tools_json.h"

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

  print_toolcall_log("cmd", {{"command", command},
                             {"working_directory", working_directory},
                             {"timeout", timeout_val == $timeout_infinite
                                             ? "infinite"
                                             : std::to_string(timeout_val)},
                             {"requires_confirmation",
                              requires_confirmation ? "true" : "false"}});

  // Check if user confirmation is required
  if (requires_confirmation) {
    std::string answer = ai::utils::getUserInputFromTerminal(
        "\n⚠️  CMD command requires confirmation:\n   " + command +
        "\n   Execute? (y/n): ");
    if (answer != "y" && answer != "Y" && answer != "yes" && answer != "Yes") {
      return "Command cancelled by user: " + command;
    }
  }

  subprocess::buffer out_buf{[](const unsigned char* data, size_t size) {
    std::cout.write(reinterpret_cast<const char*>(data), size);
  }};
  subprocess::buffer err_buf{[](const unsigned char* data, size_t size) {
    std::cerr.write(reinterpret_cast<const char*>(data), size);
  }};

  auto ret = subprocess::run("cmd", "/c", command, $stdout > out_buf,
                             $stderr > err_buf, $timeout = timeout_val,
                             $cwd = working_directory);

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
    result = "Command executed successfully with no output. Exit code: " +
             std::to_string(ret);
  }

  return result;
}

std::string_view get_cmd_tools() { return cmd_tools_json_str; }

void regist_cmd_tools() { regist_tool_calls("cmd", cmd); }

// Self-register the category at static-init time
static bool _cmd_tool_category_registered =
    regist_tool_category("cmd", get_cmd_tools, regist_cmd_tools);
