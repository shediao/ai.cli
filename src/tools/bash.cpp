#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <subprocess/subprocess.hpp>

#include "bash_tools_json.h"
#include "logging.h"
#include "tool_calls.h"

std::string bash(nlohmann::json const& args) {
  LOG(INFO) << "call bash(" << args.dump() << ")";
  if (args.is_object() && args.contains("command") &&
      args["command"].is_string()) {
    std::string command = args["command"].get<std::string>();

    // Check if user confirmation is required
    if (args.contains("requires_confirmation") &&
        args["requires_confirmation"].is_boolean() &&
        args["requires_confirmation"].get<bool>()) {
      std::cerr << "\n⚠️  Bash command requires confirmation:\n"
                << "   " << command << "\n"
                << "   Execute? (y/n): ";
      std::string answer;
      std::getline(std::cin, answer);
      if (answer != "y" && answer != "Y" && answer != "yes" &&
          answer != "Yes") {
        return "Command cancelled by user: " + command;
      }
    }

    subprocess::buffer out_buf;
    subprocess::buffer err_buf;

    using namespace subprocess::named_arguments;
    using subprocess::run;

    int ret = run("bash", "-c", command, std_out > out_buf,
                  std_err > err_buf);

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
  return "tool_calls bash arguments is invalid. Expected: {\"command\": \"...\"}";
}

const std::string_view get_bash_tools() { return bash_tools_json_str; }

void regist_bash_tools() { regist_tool_calls("bash", bash); }
