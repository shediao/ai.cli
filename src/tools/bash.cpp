#include <environment/environment.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <subprocess/subprocess.hpp>

#include "ai/logging.h"
#include "ai/tool_calls.h"
#include "ai/utils.h"
#include "bash_tools_json.h"

std::string bash(nlohmann::json const& args) {
  LOG(INFO) << "call bash(" << args.dump() << ")";
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

  // Check if user confirmation is required
  if (args.contains("requires_confirmation") &&
      args["requires_confirmation"].is_boolean() &&
      args["requires_confirmation"].get<bool>()) {
    std::string answer = ai::utils::getUserInputFromTerminal(
        "\n⚠️  Bash command requires confirmation:\n   " + command +
        "\n   Execute? (y/n): ");
    if (answer != "y" && answer != "Y" && answer != "yes" && answer != "Yes") {
      return "Command cancelled by user: " + command;
    }
  }
  std::string bash_cmd = "bash";
#if defined(_WIN32)
  if (auto shell = env::get("SHELL").value_or("");
      (shell.ends_with("sh") || shell.ends_with("sh.exe"))) {
    if (shell.ends_with("bash") || shell.ends_with("bash.exe")) {
      bash_cmd = shell;
    }
  } else {
    for (auto const& path : env::path()) {
      if (auto p = std::filesystem::path(path) / "bash.exe"; exists(p)) {
        bash_cmd = p.string();
        break;
      }
    }
  }
#else
  bool bash_exists = false;
  for (auto const& path : env::path()) {
    if (auto p = std::filesystem::path(path) / "bash"; exists(p)) {
      bash_exists = true;
      break;
    }
  }
  if (!bash_exists) {
    if (auto shell = env::get("SHELL"); shell.has_value()) {
      bash_cmd = shell.value();
    }
  }
#endif
  subprocess::buffer out_buf{[](const unsigned char* data, size_t size) {
    std::cout.write(reinterpret_cast<const char*>(data), size);
  }};
  subprocess::buffer err_buf{[](const unsigned char* data, size_t size) {
    std::cerr.write(reinterpret_cast<const char*>(data), size);
  }};
  auto timeout_val =
      args.contains("timeout") && args["timeout"].is_number_integer()
          ? args["timeout"].get<int>()
          : $timeout_infinite;
  std::string working_directory =
      args.contains("working_directory") &&
              args["working_directory"].is_string()
          ? args["working_directory"].get<std::string>()
          : "";
  auto ret = subprocess::run(bash_cmd, "-c", command, $stdout > out_buf,
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

std::string_view get_bash_tools() { return bash_tools_json_str; }

void regist_bash_tools() { regist_tool_calls("bash", bash); }

// Self-register the category at static-init time
static bool _bash_tool_category_registered =
    regist_tool_category("bash", get_bash_tools, regist_bash_tools);
