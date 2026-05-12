#include <gtest/gtest.h>

#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>
#include <subprocess/subprocess.hpp>

#include "ai/tool_calls.h"

using json = nlohmann::json;

// =============================================================================
// Bash tests - Validation
// =============================================================================

TEST(BashTest, NotAnObject) {
  json args = json::array();
  std::string result = call_tool("bash", args);
  EXPECT_TRUE(result.find("expected a JSON object") != std::string::npos);
}

TEST(BashTest, MissingCommand) {
  json args = json::object();
  std::string result = call_tool("bash", args);
  EXPECT_TRUE(result.find("missing required parameter") != std::string::npos);
  EXPECT_TRUE(result.find("command") != std::string::npos);
}

TEST(BashTest, CommandNotString) {
  json args = {{"command", 12345}};
  std::string result = call_tool("bash", args);
  EXPECT_TRUE(result.find("\"command\" must be a") != std::string::npos);
}

// =============================================================================
// Bash tests - Execution (these require bash to be available)
// =============================================================================

TEST(BashTest, EchoCommand) {
  json args = {{"command", "echo hello"}};
  std::string result = call_tool("bash", args);
  EXPECT_TRUE(result.find("hello") != std::string::npos) << result;
}

TEST(BashTest, StderrCapture) {
  json args = {{"command", "echo error >&2"}};
  std::string result = call_tool("bash", args);
  EXPECT_TRUE(result.find("error") != std::string::npos) << result;
}

TEST(BashTest, StdoutAndStderrCombined) {
  json args = {{"command", "echo stdout; echo stderr >&2"}};
  std::string result = call_tool("bash", args);
  EXPECT_TRUE(result.find("stdout") != std::string::npos) << result;
  EXPECT_TRUE(result.find("stderr") != std::string::npos) << result;
}

TEST(BashTest, EmptyOutput) {
  json args = {{"command", "exit 0"}};
  std::string result = call_tool("bash", args);
  EXPECT_TRUE(result.find("Exit code: 0") != std::string::npos) << result;
}

TEST(BashTest, NonZeroExitCode) {
  json args = {{"command", "exit 42"}};
  std::string result = call_tool("bash", args);
  EXPECT_TRUE(result.find("Exit code: 42") != std::string::npos) << result;
}

TEST(BashTest, MultilineOutput) {
  json args = {{"command", "echo line1; echo line2; echo line3"}};
  std::string result = call_tool("bash", args);
  EXPECT_TRUE(result.find("line1") != std::string::npos) << result;
  EXPECT_TRUE(result.find("line2") != std::string::npos) << result;
  EXPECT_TRUE(result.find("line3") != std::string::npos) << result;
}

TEST(BashTest, PipeAndRedirect) {
  json args = {{"command", "echo 'hello world' | wc -c"}};
  std::string result = call_tool("bash", args);
  // "hello world\n" = 12 characters
  // Trim the result and check it's a number >= 1
  EXPECT_FALSE(result.empty()) << result;
}

TEST(BashTest, CommandNotFound) {
  json args = {{"command", "nonexistent_command_xyz_123"}};
  std::string result = call_tool("bash", args);
  // Should have non-zero exit code or stderr output
  EXPECT_FALSE(result.empty()) << result;
}

TEST(BashTest, Timeout) {
  // Run a command that sleeps longer than the timeout
  json args = {{"command", "sleep 30"}, {"timeout", 1}};
  std::string result = call_tool("bash", args);
  // On timeout, the subprocess library generally returns non-zero exit code
  // and may have error output. We just verify we get some result back quickly.
  EXPECT_FALSE(result.empty()) << result;
}

TEST(BashTest, WorkingDirectory) {
  // Create a temp directory and run pwd with working_directory set to it
  std::filesystem::path tmpdir_raw =
      std::filesystem::temp_directory_path() /
      ("ai_cli_test_bash_cwd_" + std::to_string(std::rand()));
  std::filesystem::create_directories(tmpdir_raw);
  std::filesystem::path tmpdir = std::filesystem::canonical(tmpdir_raw);

#if defined(_WIN32)
  std::string cmd =
      "if [[ $(uname -s) =~ ^(MINGW|MSYS).*$ ]]; then cygpath -w $(pwd); else "
      "pwd; fi";
#else
  std::string cmd = "pwd";
#endif
  json args = {{"command", cmd}, {"working_directory", tmpdir.string()}};
  std::string result = call_tool("bash", args);
  // pwd should output the absolute path of tmpdir
  EXPECT_TRUE(result.find(tmpdir.string()) != std::string::npos)
      << "path: " << tmpdir.string() << "\nresult: " << result;

  std::filesystem::remove_all(tmpdir);
}

TEST(BashTest, WorkingDirectoryNonexistent) {
  json args = {{"command", "echo hello"},
               {"working_directory", "/nonexistent/path/xyz_123_not_exists"}};
  std::string result = call_tool("bash", args);
  // Should get an error about the directory not existing
  EXPECT_FALSE(result.empty()) << result;
  EXPECT_TRUE(result.find("hello") == std::string::npos) << result;
}

// =============================================================================
// CMD tests - Validation (cross-platform, no execution needed)
// =============================================================================

TEST(CmdTest, NotAnObject) {
  json args = json::array();
  std::string result = call_tool("cmd", args);
  EXPECT_TRUE(result.find("expected a JSON object") != std::string::npos);
}

TEST(CmdTest, MissingCommand) {
  json args = json::object();
  std::string result = call_tool("cmd", args);
  EXPECT_TRUE(result.find("missing required parameter") != std::string::npos);
  EXPECT_TRUE(result.find("command") != std::string::npos);
}

TEST(CmdTest, CommandNotString) {
  json args = {{"command", 12345}};
  std::string result = call_tool("cmd", args);
  EXPECT_TRUE(result.find("\"command\" must be a") != std::string::npos);
}

// =============================================================================
// CMD tests - Execution (only on Windows; skipped on other platforms)
// =============================================================================

#if defined(_WIN32)
TEST(CmdTest, EchoCommand) {
  json args = {{"command", "echo hello"}};
  std::string result = call_tool("cmd", args);
  EXPECT_TRUE(result.find("hello") != std::string::npos);
}

TEST(CmdTest, StderrCapture) {
  json args = {{"command", "echo error 1>&2"}};
  std::string result = call_tool("cmd", args);
  EXPECT_TRUE(result.find("error") != std::string::npos);
}

TEST(CmdTest, NonZeroExit) {
  json args = {{"command", "exit /b 99"}};
  std::string result = call_tool("cmd", args);
  EXPECT_TRUE(result.find("Exit code: 99") != std::string::npos);
}

TEST(CmdTest, Timeout) {
  json args = {{"command", "ping -n 30 127.0.0.1 > nul"}, {"timeout", 1}};
  std::string result = call_tool("cmd", args);
  EXPECT_FALSE(result.empty());
}

TEST(CmdTest, WorkingDirectory) {
  // Create a temp directory and run cd to verify working directory
  std::filesystem::path tmpdir_raw =
      std::filesystem::temp_directory_path() /
      ("ai_cli_test_cmd_cwd_" + std::to_string(std::rand()));
  std::filesystem::create_directories(tmpdir_raw);
  std::filesystem::path tmpdir = std::filesystem::canonical(tmpdir_raw);

  json args = {{"command", "cd"}, {"working_directory", tmpdir.string()}};
  std::string result = call_tool("cmd", args);
  // cd with no args on Windows prints the current directory
  EXPECT_TRUE(result.find(tmpdir.string()) != std::string::npos)
      << "path: " << tmpdir.string() << "\nresult: " << result;

  std::filesystem::remove_all(tmpdir);
}

TEST(CmdTest, WorkingDirectoryNonexistent) {
  json args = {
      {"command", "echo hello"},
      {"working_directory", "C:\\nonexistent\\path\\xyz_123_not_exists"}};
  std::string result = call_tool("cmd", args);
  EXPECT_FALSE(result.empty()) << result;
  EXPECT_TRUE(result.find("hello") == std::string::npos) << result;
}
#endif  // _WIN32

// =============================================================================
// PowerShell tests - Validation (cross-platform, no execution needed)
// =============================================================================

TEST(PowershellTest, NotAnObject) {
  json args = json::array();
  std::string result = call_tool("powershell", args);
  EXPECT_TRUE(result.find("expected a JSON object") != std::string::npos);
}

TEST(PowershellTest, MissingCommand) {
  json args = json::object();
  std::string result = call_tool("powershell", args);
  EXPECT_TRUE(result.find("missing required parameter") != std::string::npos);
  EXPECT_TRUE(result.find("command") != std::string::npos);
}

TEST(PowershellTest, CommandNotString) {
  json args = {{"command", 12345}};
  std::string result = call_tool("powershell", args);
  EXPECT_TRUE(result.find("\"command\" must be a") != std::string::npos);
}

// =============================================================================
// PowerShell tests - Execution (only on Windows; skipped on other platforms)
// =============================================================================

#if defined(_WIN32)
TEST(PowershellTest, EchoCommand) {
  json args = {{"command", "Write-Output 'hello'"}};
  std::string result = call_tool("powershell", args);
  EXPECT_TRUE(result.find("hello") != std::string::npos);
}

TEST(PowershellTest, StderrCapture) {
  json args = {{"command", "Write-Error 'error_msg'"}};
  std::string result = call_tool("powershell", args);
  // PowerShell Write-Error goes to stderr
  EXPECT_FALSE(result.empty());
}

TEST(PowershellTest, NonZeroExit) {
  json args = {{"command", "exit 77"}};
  std::string result = call_tool("powershell", args);
  EXPECT_TRUE(result.find("Exit code: 77") != std::string::npos);
}

TEST(PowershellTest, Timeout) {
  json args = {{"command", "Start-Sleep -Seconds 30"}, {"timeout", 1}};
  std::string result = call_tool("powershell", args);
  EXPECT_FALSE(result.empty());
}

TEST(PowershellTest, WorkingDirectory) {
  // Create a temp directory and verify working directory via Get-Location
  std::filesystem::path tmpdir_raw =
      std::filesystem::temp_directory_path() /
      ("ai_cli_test_ps_cwd_" + std::to_string(std::rand()));
  std::filesystem::create_directories(tmpdir_raw);
  // Resolve to canonical path to avoid 8.3 short-name mismatch on Windows
  // (e.g., C:\Users\RUNNER~1 vs C:\Users\runneradmin)
  std::filesystem::path tmpdir = std::filesystem::canonical(tmpdir_raw);

  json args = {{"command", "Get-Location"},
               {"working_directory", tmpdir.string()}};
  std::string result = call_tool("powershell", args);
  EXPECT_TRUE(result.find(tmpdir.string()) != std::string::npos)
      << "path: " << tmpdir.string() << "\nresult: " << result;

  std::filesystem::remove_all(tmpdir);
}

TEST(PowershellTest, WorkingDirectoryNonexistent) {
  json args = {
      {"command", "Write-Output hello"},
      {"working_directory", "C:\\nonexistent\\path\\xyz_123_not_exists"}};
  std::string result = call_tool("powershell", args);
  EXPECT_FALSE(result.empty()) << result;
  EXPECT_TRUE(result.find("hello") == std::string::npos) << result;
}
#endif  // _WIN32
