#include <environment/environment.hpp>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>

#include "ai/logging.h"
#include "ai/tool_calls.h"
#include "default_tools_json.h"

// ── get_working_directory ─────────────────────────────────────────────
std::string get_working_directory(nlohmann::json const& args) {
  LOG(INFO) << "call get_working_directory(" << args.dump() << ")";
  if (!args.is_object()) {
    return "function get_working_directory arguments is invalid: expected a "
           "JSON object.";
  }

  std::error_code err;
  std::string cwd = std::filesystem::current_path(err).string();
  if (err) {
    return "Error getting current working directory: " + err.message();
  }
  return cwd;
}

// ── set_working_directory ─────────────────────────────────────────────
std::string set_working_directory(nlohmann::json const& args) {
  LOG(INFO) << "call set_working_directory(" << args.dump() << ")";
  if (!args.is_object()) {
    return "function set_working_directory arguments is invalid: expected a "
           "JSON object.";
  }
  if (!args.contains("path")) {
    return "function set_working_directory arguments is invalid: missing "
           "required parameter \"path\".";
  }
  if (!args["path"].is_string()) {
    return "function set_working_directory arguments is invalid: \"path\" "
           "must be a string.";
  }

  std::string path = args["path"].get<std::string>();
  std::error_code err;
  std::filesystem::current_path(path, err);
  if (err) {
    return "Error changing working directory to \"" + path +
           "\": " + err.message();
  }

  std::string new_cwd = std::filesystem::current_path(err).string();
  return "Successfully changed working directory to: " + new_cwd;
}

// ── get_environment_variable ──────────────────────────────────────────
std::string get_environment_variable(nlohmann::json const& args) {
  LOG(INFO) << "call get_environment_variable(" << args.dump() << ")";
  if (!args.is_object()) {
    return "function get_environment_variable arguments is invalid: expected "
           "a JSON object.";
  }
  if (!args.contains("name")) {
    return "function get_environment_variable arguments is invalid: missing "
           "required parameter \"name\".";
  }
  if (!args["name"].is_string()) {
    return "function get_environment_variable arguments is invalid: \"name\" "
           "must be a string.";
  }

  std::string name = args["name"].get<std::string>();
  auto value = env::get(name);
  if (!value.has_value()) {
    return "Environment variable \"" + name + "\" is not set.";
  }
  return value.value();
}

// ── set_environment_variable ──────────────────────────────────────────
std::string set_environment_variable(nlohmann::json const& args) {
  LOG(INFO) << "call set_environment_variable(" << args.dump() << ")";
  if (!args.is_object()) {
    return "function set_environment_variable arguments is invalid: expected "
           "a JSON object.";
  }
  if (!args.contains("name")) {
    return "function set_environment_variable arguments is invalid: missing "
           "required parameter \"name\".";
  }
  if (!args["name"].is_string()) {
    return "function set_environment_variable arguments is invalid: \"name\" "
           "must be a string.";
  }
  if (!args.contains("value")) {
    return "function set_environment_variable arguments is invalid: missing "
           "required parameter \"value\".";
  }
  if (!args["value"].is_string()) {
    return "function set_environment_variable arguments is invalid: "
           "\"value\" must be a string.";
  }

  std::string name = args["name"].get<std::string>();
  std::string value = args["value"].get<std::string>();

  // setenv on POSIX, _putenv on Windows
#if defined(_WIN32)
  std::string env_str = name + "=" + value;
  if (_putenv(env_str.c_str()) != 0) {
    return "Error setting environment variable \"" + name + "\".";
  }
#else
  if (setenv(name.c_str(), value.c_str(), 1) != 0) {
    return "Error setting environment variable \"" + name + "\".";
  }
#endif

  return "Successfully set environment variable \"" + name + "\" to \"" +
         value + "\".";
}

// ── get_shell ─────────────────────────────────────────────────────────
std::string get_shell(nlohmann::json const& args) {
  LOG(INFO) << "call get_shell(" << args.dump() << ")";
  if (!args.is_object()) {
    return "function get_shell arguments is invalid: expected a JSON object.";
  }

#if defined(_WIN32)
  auto shell = env::get("COMSPEC").value_or("cmd.exe");
  if (auto sh = env::get("SHELL"); sh.has_value()) {
    if (auto& s = sh.value(); s.ends_with("sh.exe") || s.ends_with("sh")) {
      shell = s;
    }
  }
#else
  auto shell = env::get("SHELL").value_or("/bin/bash");
#endif

  return shell;
}

// ── get_operating_system ──────────────────────────────────────────────
std::string get_operating_system(nlohmann::json const& args) {
  LOG(INFO) << "call get_operating_system(" << args.dump() << ")";
  if (!args.is_object()) {
    return "function get_operating_system arguments is invalid: expected a "
           "JSON object.";
  }

  nlohmann::json info;

#if defined(_WIN32)
  info["name"] = "Windows";
#elif defined(__APPLE__)
  info["name"] = "macOS";
#elif defined(__linux__)
  info["name"] = "Linux";
#elif defined(__FreeBSD__)
  info["name"] = "FreeBSD";
#else
  info["name"] = "Unknown";
#endif

#if defined(__x86_64__) || defined(_M_X64)
  info["architecture"] = "x86_64";
#elif defined(__aarch64__) || defined(_M_ARM64)
  info["architecture"] = "arm64";
#elif defined(__i386__) || defined(_M_IX86)
  info["architecture"] = "x86";
#else
  info["architecture"] = "unknown";
#endif

  // Add a more detailed OS description where possible
#if defined(__APPLE__)
  // Run sw_vers to get macOS version
  {
    FILE* pipe = popen("sw_vers -productVersion 2>/dev/null", "r");
    if (pipe) {
      char buf[128];
      std::string version;
      while (fgets(buf, sizeof(buf), pipe) != nullptr) {
        version += buf;
      }
      pclose(pipe);
      // Trim trailing newline
      while (!version.empty() &&
             (version.back() == '\n' || version.back() == '\r')) {
        version.pop_back();
      }
      if (!version.empty()) {
        info["version"] = version;
      }
    }
  }
  info["description"] = "macOS (Darwin)";
#elif defined(__linux__)
  info["description"] = "Linux";
#elif defined(_WIN32)
  info["description"] = "Windows";
#endif

  return info.dump();
}

// ── Category wiring ───────────────────────────────────────────────────

std::string_view get_default_tools() { return default_tools_json_str; }

void regist_default_tools() {
  regist_tool_calls("get_working_directory", get_working_directory);
  regist_tool_calls("set_working_directory", set_working_directory);
  regist_tool_calls("get_environment_variable", get_environment_variable);
  regist_tool_calls("set_environment_variable", set_environment_variable);
  regist_tool_calls("get_shell", get_shell);
  regist_tool_calls("get_operating_system", get_operating_system);
}

// Self-register the category at static-init time
static bool _default_tool_category_registered =
    regist_tool_category("default", get_default_tools, regist_default_tools);
