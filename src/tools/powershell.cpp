#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <subprocess/subprocess.hpp>

#include "ai/logging.h"
#include "ai/tool_calls.h"
#include "powershell_tools_json.h"

std::string powershell(nlohmann::json const& args) {
  LOG(INFO) << "call powershell(" << args.dump() << ")";
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

  // Check if user confirmation is required
  if (args.contains("requires_confirmation") &&
      args["requires_confirmation"].is_boolean() &&
      args["requires_confirmation"].get<bool>()) {
    std::cerr << "\n⚠️  PowerShell command requires confirmation:\n"
              << "   " << command << "\n"
              << "   Execute? (y/n): ";
    std::string answer;
    std::getline(std::cin, answer);
    if (answer != "y" && answer != "Y" && answer != "yes" && answer != "Yes") {
      return "Command cancelled by user: " + command;
    }
  }

  subprocess::buffer out_buf;
  subprocess::buffer err_buf;

  using namespace subprocess::named_arguments;
  using subprocess::run;

  int ret = run("powershell", "-NoProfile", "-Command", command,
                std_out > out_buf, std_err > err_buf);

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

std::string_view get_powershell_tools() { return powershell_tools_json_str; }

void regist_powershell_tools() { regist_tool_calls("powershell", powershell); }

// Self-register the category at static-init time
static bool _powershell_tool_category_registered = regist_tool_category(
    "powershell", get_powershell_tools, regist_powershell_tools);
