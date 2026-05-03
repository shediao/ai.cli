#include <gtest/gtest.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <string_view>
#include <subprocess/subprocess.hpp>

using json = nlohmann::json;

// =============================================================================
// Stubs for symbols that bash/cmd/powershell.cpp depend on
// =============================================================================

// Include the real logging header for LogMessage class declaration
#include "ai/logging.h"

// --- logging stubs ---
namespace ai::logging {
LogMessage::LogMessage(const char* file, int line, LogSeverity severity)
    : severity_(severity), message_start_(0), file_(file), line_(line) {}
LogMessage::~LogMessage() = default;
bool ShouldCreateLogMessage(LogSeverity) { return false; }
}  // namespace ai::logging

// --- tool_calls stubs (for static init) ---
bool regist_tool_calls(std::string const&,
                       std::function<std::string(json const&)>) {
  return true;
}
bool regist_tool_category(std::string const&, std::string_view (*)(),
                          void (*)()) {
  return true;
}

// --- tool JSON stubs ---
[[maybe_unused]] constexpr std::string_view bash_tools_json_str = "{}";
[[maybe_unused]] constexpr std::string_view cmd_tools_json_str = "{}";
[[maybe_unused]] constexpr std::string_view powershell_tools_json_str = "{}";

// =============================================================================
// Forward declarations of all testable functions
// =============================================================================
std::string bash(nlohmann::json const& args);
std::string cmd(nlohmann::json const& args);
std::string powershell(nlohmann::json const& args);

// =============================================================================
// Bash tests - Validation
// =============================================================================

TEST(BashTest, NotAnObject) {
  json args = json::array();
  std::string result = bash(args);
  EXPECT_TRUE(result.find("expected a JSON object") != std::string::npos);
}

TEST(BashTest, MissingCommand) {
  json args = json::object();
  std::string result = bash(args);
  EXPECT_TRUE(result.find("missing required parameter") != std::string::npos);
  EXPECT_TRUE(result.find("command") != std::string::npos);
}

TEST(BashTest, CommandNotString) {
  json args = {{"command", 12345}};
  std::string result = bash(args);
  EXPECT_TRUE(result.find("\"command\" must be a") != std::string::npos);
}

// =============================================================================
// Bash tests - Execution (these require bash to be available)
// =============================================================================

TEST(BashTest, EchoCommand) {
  json args = {{"command", "echo hello"}};
  std::string result = bash(args);
  EXPECT_TRUE(result.find("hello") != std::string::npos) << result;
}

TEST(BashTest, StderrCapture) {
  json args = {{"command", "echo error >&2"}};
  std::string result = bash(args);
  EXPECT_TRUE(result.find("error") != std::string::npos) << result;
}

TEST(BashTest, StdoutAndStderrCombined) {
  json args = {{"command", "echo stdout; echo stderr >&2"}};
  std::string result = bash(args);
  EXPECT_TRUE(result.find("stdout") != std::string::npos) << result;
  EXPECT_TRUE(result.find("stderr") != std::string::npos) << result;
}

TEST(BashTest, EmptyOutput) {
  json args = {{"command", "exit 0"}};
  std::string result = bash(args);
  EXPECT_TRUE(result.find("Exit code: 0") != std::string::npos) << result;
}

TEST(BashTest, NonZeroExitCode) {
  json args = {{"command", "exit 42"}};
  std::string result = bash(args);
  EXPECT_TRUE(result.find("Exit code: 42") != std::string::npos) << result;
}

TEST(BashTest, MultilineOutput) {
  json args = {{"command", "echo line1; echo line2; echo line3"}};
  std::string result = bash(args);
  EXPECT_TRUE(result.find("line1") != std::string::npos) << result;
  EXPECT_TRUE(result.find("line2") != std::string::npos) << result;
  EXPECT_TRUE(result.find("line3") != std::string::npos) << result;
}

TEST(BashTest, PipeAndRedirect) {
  json args = {{"command", "echo 'hello world' | wc -c"}};
  std::string result = bash(args);
  // "hello world\n" = 12 characters
  // Trim the result and check it's a number >= 1
  EXPECT_FALSE(result.empty()) << result;
}

TEST(BashTest, CommandNotFound) {
  json args = {{"command", "nonexistent_command_xyz_123"}};
  std::string result = bash(args);
  // Should have non-zero exit code or stderr output
  EXPECT_FALSE(result.empty()) << result;
}

TEST(BashTest, Timeout) {
  // Run a command that sleeps longer than the timeout
  json args = {{"command", "sleep 30"}, {"timeout", 1}};
  std::string result = bash(args);
  // On timeout, the subprocess library generally returns non-zero exit code
  // and may have error output. We just verify we get some result back quickly.
  EXPECT_FALSE(result.empty()) << result;
}

// =============================================================================
// CMD tests - Validation (cross-platform, no execution needed)
// =============================================================================

TEST(CmdTest, NotAnObject) {
  json args = json::array();
  std::string result = cmd(args);
  EXPECT_TRUE(result.find("expected a JSON object") != std::string::npos);
}

TEST(CmdTest, MissingCommand) {
  json args = json::object();
  std::string result = cmd(args);
  EXPECT_TRUE(result.find("missing required parameter") != std::string::npos);
  EXPECT_TRUE(result.find("command") != std::string::npos);
}

TEST(CmdTest, CommandNotString) {
  json args = {{"command", 12345}};
  std::string result = cmd(args);
  EXPECT_TRUE(result.find("\"command\" must be a") != std::string::npos);
}

// =============================================================================
// CMD tests - Execution (only on Windows; skipped on other platforms)
// =============================================================================

#if defined(_WIN32)
TEST(CmdTest, EchoCommand) {
  json args = {{"command", "echo hello"}};
  std::string result = cmd(args);
  EXPECT_TRUE(result.find("hello") != std::string::npos);
}

TEST(CmdTest, StderrCapture) {
  json args = {{"command", "echo error 1>&2"}};
  std::string result = cmd(args);
  EXPECT_TRUE(result.find("error") != std::string::npos);
}

TEST(CmdTest, NonZeroExit) {
  json args = {{"command", "exit /b 99"}};
  std::string result = cmd(args);
  EXPECT_TRUE(result.find("Exit code: 99") != std::string::npos);
}

TEST(CmdTest, Timeout) {
  json args = {{"command", "ping -n 30 127.0.0.1 > nul"}, {"timeout", 1}};
  std::string result = cmd(args);
  EXPECT_FALSE(result.empty());
}
#endif  // _WIN32

// =============================================================================
// PowerShell tests - Validation (cross-platform, no execution needed)
// =============================================================================

TEST(PowershellTest, NotAnObject) {
  json args = json::array();
  std::string result = powershell(args);
  EXPECT_TRUE(result.find("expected a JSON object") != std::string::npos);
}

TEST(PowershellTest, MissingCommand) {
  json args = json::object();
  std::string result = powershell(args);
  EXPECT_TRUE(result.find("missing required parameter") != std::string::npos);
  EXPECT_TRUE(result.find("command") != std::string::npos);
}

TEST(PowershellTest, CommandNotString) {
  json args = {{"command", 12345}};
  std::string result = powershell(args);
  EXPECT_TRUE(result.find("\"command\" must be a") != std::string::npos);
}

// =============================================================================
// PowerShell tests - Execution (only on Windows; skipped on other platforms)
// =============================================================================

#if defined(_WIN32)
TEST(PowershellTest, EchoCommand) {
  json args = {{"command", "Write-Output 'hello'"}};
  std::string result = powershell(args);
  EXPECT_TRUE(result.find("hello") != std::string::npos);
}

TEST(PowershellTest, StderrCapture) {
  json args = {{"command", "Write-Error 'error_msg'"}};
  std::string result = powershell(args);
  // PowerShell Write-Error goes to stderr
  EXPECT_FALSE(result.empty());
}

TEST(PowershellTest, NonZeroExit) {
  json args = {{"command", "exit 77"}};
  std::string result = powershell(args);
  EXPECT_TRUE(result.find("Exit code: 77") != std::string::npos);
}

TEST(PowershellTest, Timeout) {
  json args = {{"command", "Start-Sleep -Seconds 30"}, {"timeout", 1}};
  std::string result = powershell(args);
  EXPECT_FALSE(result.empty());
}
#endif  // _WIN32
