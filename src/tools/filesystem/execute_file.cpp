#include <chrono>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <subprocess/subprocess.hpp>
#include <vector>

#include "ai/function.h"

namespace ai {

extern std::string expand_tilde(std::string const& path);
extern std::optional<std::string> resolve_path(nlohmann::json const& args);

namespace {
std::string execute_file(nlohmann::json const& args) {
  if (!args.is_object()) {
    return "function execute_file arguments is invalid: expected a JSON "
           "object.";
  }
  auto path_opt = resolve_path(args);
  if (!path_opt.has_value()) {
    return "function execute_file arguments is invalid: missing required "
           "parameter \"path\".";
  }
  if (path_opt->empty()) {
    return "function execute_file arguments is invalid: \"path\" must be a "
           "string.";
  }

  std::string path = std::move(*path_opt);
  path = expand_tilde(path);

  // Build the command vector
  std::vector<std::string> cmd_args;
  cmd_args.push_back(path);
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
  print_toolcall_log("execute_file",
                     {{"path", path},
                      {"working_directory", working_directory},
                      {"timeout", timeout_val == timeout_infinite
                                      ? "infinite"
                                      : std::to_string(timeout_val)},
                      {"args", args_str}});

  auto start = std::chrono::steady_clock::now();
  auto [exit_code, out_buf, err_buf] = subprocess::capture_run(
      cmd_args, timeout = timeout_val, cwd = working_directory);
  auto elapsed = std::chrono::steady_clock::now() - start;

  std::string result;
  result += "Exit code: " + std::to_string(exit_code) + "\n";

  std::string out_str = out_buf.to_string();
  std::string err_str = err_buf.to_string();

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

class ExecuteFileFunction : public ai::Function {
 public:
  std::string call(nlohmann::json const& args) override {
    return execute_file(args);
  }
  std::string const& category() const override { return category_; }
  nlohmann::json const& schema() const override { return schema_; }

 private:
  std::string category_ = "filesystem";
  nlohmann::json schema_ = R"===(
{
  "type": "function",
  "name": "execute_file",
  "description": "Execute a file as a subprocess and capture its output. Returns the exit code, stdout, and stderr. Use this tool when you need to run an executable or script file.",
  "parameters": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string",
        "description": "The path to the executable file to run."
      },
      "args": {
        "type": "array",
        "items": {
          "type": "string"
        },
        "description": "Optional arguments to pass to the executable."
      },
      "timeout": {
        "type": "integer",
        "description": "Optional timeout in seconds. If the executable does not complete within this time, it will be terminated. If not provided, no timeout is applied."
      },
      "working_directory": {
        "type": "string",
        "description": "Optional working directory for the executable. If provided, the executable will be run in this directory. Can be an absolute path or a relative path (resolved against the current working directory)."
      }
    },
    "required": ["path"]
  }
}
)==="_json;
};

AUTO_REGISTER(ExecuteFileFunction);

}  // namespace ai
