#include <environment/environment.hpp>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>

#include "ai/function.h"

namespace ai {

namespace {
// ── get_working_directory ─────────────────────────────────────────────
std::string get_working_directory(nlohmann::json const& args) {
  if (!args.is_object()) {
    return "function get_working_directory arguments is invalid: expected a "
           "JSON object.";
  }

  print_toolcall_log("get_working_directory", {});

  std::error_code err;
  std::string cwd = std::filesystem::current_path(err).string();
  if (err) {
    return "Error getting current working directory: " + err.message();
  }
  return cwd;
}

// ── get_environment_variable ──────────────────────────────────────────
std::string get_environment_variable(nlohmann::json const& args) {
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
  print_toolcall_log("get_environment_variable", {{"name", name}});
  auto value = env::get(name);
  if (!value.has_value()) {
    return "Environment variable \"" + name + "\" is not set.";
  }
  return value.value();
}

// ── set_environment_variable ──────────────────────────────────────────
std::string set_environment_variable(nlohmann::json const& args) {
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
  print_toolcall_log("set_environment_variable",
                     {{"name", name}, {"value", value}});

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
  if (!args.is_object()) {
    return "function get_shell arguments is invalid: expected a JSON object.";
  }

  print_toolcall_log("get_shell", {});

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
  if (!args.is_object()) {
    return "function get_operating_system arguments is invalid: expected a "
           "JSON object.";
  }

  print_toolcall_log("get_operating_system", {});

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
}  // namespace

// ── Category wiring ───────────────────────────────────────────────────

class GetWorkingDirectoryFunction : public ai::Function {
 public:
  std::string call(nlohmann::json const& args) override {
    return get_working_directory(args);
  }
  std::string const& category() const override { return category_; }
  nlohmann::json const& schema() const override { return schema_; }

 private:
  std::string category_ = "default";
  nlohmann::json schema_ = R"(
{
  "type": "function",
  "name": "get_working_directory",
  "description": "Get the current session working directory path. Returns the absolute path of the current working directory for the CLI session. Use this when you need to know where the user is currently working.",
  "parameters": {
    "type": "object",
    "properties": {},
    "required": []
  }
}
)"_json;
};

class GetEnvironmentVariableFunction : public ai::Function {
 public:
  std::string call(nlohmann::json const& args) override {
    return get_environment_variable(args);
  }
  std::string const& category() const override { return category_; }
  nlohmann::json const& schema() const override { return schema_; }

 private:
  std::string category_ = "default";
  nlohmann::json schema_ = R"(
{
  "type": "function",
  "name": "get_environment_variable",
  "description": "Get the value of an environment variable from the current session. Returns the value of the specified environment variable, or an empty string if not set.",
  "parameters": {
    "type": "object",
    "properties": {
      "name": {
        "type": "string",
        "description": "The name of the environment variable to retrieve (e.g., 'HOME', 'PATH', 'USER')."
      }
    },
    "required": ["name"]
  }
}
)"_json;
};

class SetEnvironmentVariableFunction : public ai::Function {
 public:
  std::string call(nlohmann::json const& args) override {
    return set_environment_variable(args);
  }
  std::string const& category() const override { return category_; }
  nlohmann::json const& schema() const override { return schema_; }

 private:
  std::string category_ = "default";
  nlohmann::json schema_ = R"(
{
  "type": "function",
  "name": "set_environment_variable",
  "description": "Set or update an environment variable for the current session. This only affects the current CLI process and any child processes spawned from it. The change is not permanent and will be lost when the session ends.",
  "parameters": {
    "type": "object",
    "properties": {
      "name": {
        "type": "string",
        "description": "The name of the environment variable to set."
      },
      "value": {
        "type": "string",
        "description": "The value to assign to the environment variable."
      }
    },
    "required": ["name", "value"]
  }
}
)"_json;
};

class GetShellFunction : public ai::Function {
 public:
  std::string call(nlohmann::json const& args) override {
    return get_shell(args);
  }
  std::string const& category() const override { return category_; }
  nlohmann::json const& schema() const override { return schema_; }

 private:
  std::string category_ = "default";
  nlohmann::json schema_ = R"(
{
  "type": "function",
  "name": "get_shell",
  "description": "Get the default shell for the current session. Returns the path to the default shell executable (e.g., '/bin/bash', '/bin/zsh'). Use this to know which shell syntax to use when executing commands.",
  "parameters": {
    "type": "object",
    "properties": {},
    "required": []
  }
}
)"_json;
};

class GetOperatingSystemFunction : public ai::Function {
 public:
  std::string call(nlohmann::json const& args) override {
    return get_operating_system(args);
  }

  std::string const& category() const override { return category_; }
  nlohmann::json const& schema() const override { return schema_; }

 private:
  std::string category_ = "default";
  nlohmann::json schema_ = R"(
{
  "type": "function",
  "name": "get_operating_system",
  "description": "Get information about the operating system of the current session. Returns the OS name, version, and architecture. Use this to make OS-specific decisions when helping the user.",
  "parameters": {
    "type": "object",
    "properties": {},
    "required": []
  }
}
)"_json;
};

AUTO_REGISTER(GetWorkingDirectoryFunction);
AUTO_REGISTER(GetEnvironmentVariableFunction);
AUTO_REGISTER(SetEnvironmentVariableFunction);
AUTO_REGISTER(GetShellFunction);
AUTO_REGISTER(GetOperatingSystemFunction);

}  // namespace ai
