#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>

#include "ai/function.h"

using json = nlohmann::json;

// =============================================================================
// Helper: create a temporary file with given content, return its path
// =============================================================================
class TempTestFile {
 public:
  explicit TempTestFile(std::string const& content,
                        std::string const& suffix = ".txt")
      : path_(std::filesystem::temp_directory_path() /
              ("ai_cli_test_" + std::to_string(counter_++) + suffix)) {
    std::ofstream out(path_, std::ios::binary);
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    out.close();
  }

  ~TempTestFile() {
    std::error_code ec;
    std::filesystem::remove(path_, ec);
  }

  std::string path() const { return path_.string(); }

 private:
  std::filesystem::path path_;
  static inline int counter_ = 0;
};

// =============================================================================
// Helper: create a temporary directory, return its path
// =============================================================================
class TempTestDir {
 public:
  TempTestDir()
      : path_(std::filesystem::temp_directory_path() /
              ("ai_cli_test_dir_" + std::to_string(counter_++))) {
    std::filesystem::create_directories(path_);
  }

  ~TempTestDir() {
    std::error_code ec;
    std::filesystem::remove_all(path_, ec);
  }

  std::string path() const { return path_.string(); }

 private:
  std::filesystem::path path_;
  static inline int counter_ = 0;
};

// =============================================================================
// execute_command tests
// =============================================================================

TEST(ExecuteCommandTest, ExecutesScript) {
  TempTestDir dir;
#if defined(_WIN32)
  std::string script_path =
      (std::filesystem::path(dir.path()) / "test_script.bat").string();
  {
    std::ofstream script(script_path, std::ios::binary);
    script << "@echo off\r\necho hello\r\necho error>&2\r\nexit /b 42\r\n";
  }
#else
  std::string script_path =
      (std::filesystem::path(dir.path()) / "test_script.sh").string();
  {
    std::ofstream script(script_path, std::ios::binary);
    script << "#!/bin/sh\necho hello\necho error >&2\nexit 42\n";
  }
  std::filesystem::permissions(script_path, std::filesystem::perms::owner_exec,
                               std::filesystem::perm_options::add);
#endif

  json args = {{"path", script_path}};
  std::string result = ai::call_tool("execute_command", args);
  EXPECT_TRUE(result.find("Exit code: 42") != std::string::npos);
  EXPECT_TRUE(result.find("hello") != std::string::npos);
  EXPECT_TRUE(result.find("error") != std::string::npos);
}

TEST(ExecuteCommandTest, WithArgs) {
  TempTestDir dir;
#if defined(_WIN32)
  std::string script_path =
      (std::filesystem::path(dir.path()) / "echo_args.bat").string();
  {
    std::ofstream script(script_path, std::ios::binary);
    script << "@echo off\r\necho \"%1\"\r\n";
  }
#else
  std::string script_path =
      (std::filesystem::path(dir.path()) / "echo_args.sh").string();
  {
    std::ofstream script(script_path, std::ios::binary);
    script << "#!/bin/sh\necho \"$1\"\n";
  }
  std::filesystem::permissions(script_path, std::filesystem::perms::owner_exec,
                               std::filesystem::perm_options::add);
#endif

  json args = {{"path", script_path}, {"args", json::array({"hello_arg"})}};
  std::string result = ai::call_tool("execute_command", args);
  EXPECT_TRUE(result.find("hello_arg") != std::string::npos);
}

TEST(ExecuteCommandTest, NotAnObject) {
  json args = json::array();
  std::string result = ai::call_tool("execute_command", args);
  EXPECT_TRUE(result.find("expected a JSON object") != std::string::npos);
}

TEST(ExecuteCommandTest, MissingPath) {
  json args = json::object();
  std::string result = ai::call_tool("execute_command", args);
  EXPECT_TRUE(result.find("missing required parameter") != std::string::npos);
}

TEST(ExecuteCommandTest, WorkingDirectory) {
  TempTestDir dir;
#if defined(_WIN32)
  std::string script_path =
      (std::filesystem::path(dir.path()) / "test_cwd_script.bat").string();
  {
    std::ofstream script(script_path, std::ios::binary);
    script << "@echo off\r\ncd\r\n";
  }
#else
  std::string script_path =
      (std::filesystem::path(dir.path()) / "test_cwd_script.sh").string();
  {
    std::ofstream script(script_path, std::ios::binary);
    script << "#!/bin/sh\npwd\n";
  }
  std::filesystem::permissions(script_path, std::filesystem::perms::owner_exec,
                               std::filesystem::perm_options::add);
#endif

  TempTestDir workdir;

  json args = {{"path", script_path}, {"working_directory", workdir.path()}};
  std::string result = ai::call_tool("execute_command", args);
  EXPECT_TRUE(result.find(workdir.path()) != std::string::npos) << result;
}

TEST(ExecuteCommandTest, WorkingDirectoryNonexistent) {
  TempTestDir dir;
#if defined(_WIN32)
  std::string script_path =
      (std::filesystem::path(dir.path()) / "test_cwd_nonexistent.bat").string();
  {
    std::ofstream script(script_path, std::ios::binary);
    script << "@echo off\r\necho hello\r\n";
  }
#else
  std::string script_path =
      (std::filesystem::path(dir.path()) / "test_cwd_nonexistent.sh").string();
  {
    std::ofstream script(script_path, std::ios::binary);
    script << "#!/bin/sh\necho hello\n";
  }
  std::filesystem::permissions(script_path, std::filesystem::perms::owner_exec,
                               std::filesystem::perm_options::add);
#endif

  json args = {{"path", script_path},
               {"working_directory", "/nonexistent/path/xyz_123_not_exists"}};
  std::string result = ai::call_tool("execute_command", args);
  EXPECT_FALSE(result.empty()) << result;
  EXPECT_TRUE(result.find("hello") == std::string::npos) << result;
}
