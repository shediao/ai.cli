#include <chrono>
#include <nlohmann/json.hpp>
#include <optional>
#include <ranges>
#include <regex>
#include <string>
#include <subprocess/subprocess.hpp>
#include <vector>

#include "ai/function.h"

namespace ai {

extern std::string expand_tilde(std::string const& path);
extern std::optional<std::string> resolve_path(nlohmann::json const& args);

namespace {

// Apply line-based filters to text. Each filter in the array is applied in
// order. Each filter object must have exactly one of: head, tail, include,
// exclude.
std::string filter_lines(std::string const& text,
                         nlohmann::json const& filters) {
  if (text.empty()) {
    return text;
  }
  if (!filters.is_array()) {
    return text;
  }

  // Split text into lines using C++20 ranges
  std::vector<std::string_view> lines;
  for (auto&& rng : text | std::views::split('\n')) {
    lines.emplace_back(rng.begin(), rng.end());
  }
  // Drop trailing empty line produced by split (matches std::getline semantics)
  if (!lines.empty() && lines.back().empty()) {
    lines.pop_back();
  }

  // Apply each filter in array order
  for (auto const& filter : filters) {
    if (!filter.is_object()) {
      continue;
    }

    if (filter.contains("exclude") && filter["exclude"].is_string()) {
      std::regex re(filter["exclude"].get<std::string>(),
                    std::regex::ECMAScript);
      std::erase_if(lines, [&re](std::string_view line) {
        return std::regex_search(line.begin(), line.end(), re);
      });
    } else if (filter.contains("include") && filter["include"].is_string()) {
      std::regex re(filter["include"].get<std::string>(),
                    std::regex::ECMAScript);
      std::erase_if(lines, [&re](std::string_view line) {
        return !std::regex_search(line.begin(), line.end(), re);
      });
    } else if (filter.contains("head") && filter["head"].is_number_integer()) {
      auto v = filter["head"].get<int>();
      if (v >= 0 && static_cast<size_t>(v) < lines.size()) {
        lines.resize(static_cast<size_t>(v));
      }
    } else if (filter.contains("tail") && filter["tail"].is_number_integer()) {
      auto v = filter["tail"].get<int>();
      if (v >= 0 && static_cast<size_t>(v) < lines.size()) {
        lines.erase(lines.begin(), lines.end() - static_cast<size_t>(v));
      }
    }
  }

  // Join lines back with newlines
  std::string result;
  result.reserve(std::accumulate(
      lines.begin(), lines.end(), 0,
      [](size_t acc, std::string_view line) { return acc + line.size() + 1; }));
  for (size_t i = 0; i < lines.size(); ++i) {
    if (i > 0) {
      result.push_back('\n');
    }
    result.append(lines[i].begin(), lines[i].end());
  }
  return result;
}

std::string execute_command(nlohmann::json const& args) {
  if (!args.is_object()) {
    return "function execute_command arguments is invalid: expected a JSON "
           "object.";
  }
  auto path_opt = resolve_path(args);
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
  print_toolcall_log("execute_command",
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
  "description": "Execute a command as a subprocess and capture its output. Returns the exit code, stdout, and stderr. Use this tool when you need to run a command. If the path contains a path separator (e.g. '/', '\\'), it is treated as a file path to an executable; otherwise it is treated as a command name resolved from PATH.",
  "parameters": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string",
        "description": "The command to run. If it contains a path separator, it is treated as a file path to an executable. Otherwise it is treated as a command name resolved from PATH."
      },
      "args": {
        "type": "array",
        "items": {
          "type": "string"
        },
        "description": "Optional arguments to pass to the command."
      },
      "timeout": {
        "type": "integer",
        "description": "Optional timeout in seconds. If the command does not complete within this time, it will be terminated. If not provided, no timeout is applied."
      },
      "working_directory": {
        "type": "string",
        "description": "Optional working directory for the command. If provided, the command will be run in this directory. Can be an absolute path or a relative path (resolved against the current working directory)."
      },
      "filter": {
        "type": "array",
        "description": "Optional ordered list of filters to apply to command output (both stdout and stderr). Filters are applied in array order. When a command is expected to produce long output (e.g., builds, logs, large directory listings), ALWAYS use filters — especially tail — to limit the output to a manageable size. Avoid returning excessive output that would overflow the context window.",
        "items": {
          "oneOf": [
            {
              "type": "object",
              "properties": {
                "head": {
                  "type": "integer",
                  "description": "Keep only the first N lines of output."
                }
              },
              "required": ["head"],
              "additionalProperties": false
            },
            {
              "type": "object",
              "properties": {
                "tail": {
                  "type": "integer",
                  "description": "Keep only the last N lines of output."
                }
              },
              "required": ["tail"],
              "additionalProperties": false
            },
            {
              "type": "object",
              "properties": {
                "include": {
                  "type": "string",
                  "description": "Only keep lines matching this ECMAScript regex pattern."
                }
              },
              "required": ["include"],
              "additionalProperties": false
            },
            {
              "type": "object",
              "properties": {
                "exclude": {
                  "type": "string",
                  "description": "Remove lines matching this ECMAScript regex pattern."
                }
              },
              "required": ["exclude"],
              "additionalProperties": false
            }
          ]
        }
      }
    },
    "required": ["path"]
  }
}
)==="_json;
};

AUTO_REGISTER(ExecuteCommandFunction);

}  // namespace ai
