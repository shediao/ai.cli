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

TEST(ExecuteCommandTest, FilterHead) {
  TempTestDir dir;
#if defined(_WIN32)
  std::string script_path =
      (std::filesystem::path(dir.path()) / "filter_head.bat").string();
  {
    std::ofstream script(script_path, std::ios::binary);
    script << "@echo off\r\n"
              "echo line1\r\n"
              "echo line2\r\n"
              "echo line3\r\n"
              "echo line4\r\n"
              "echo line5\r\n";
  }
#else
  std::string script_path =
      (std::filesystem::path(dir.path()) / "filter_head.sh").string();
  {
    std::ofstream script(script_path, std::ios::binary);
    script << "#!/bin/sh\n"
              "echo line1\n"
              "echo line2\n"
              "echo line3\n"
              "echo line4\n"
              "echo line5\n";
  }
  std::filesystem::permissions(script_path, std::filesystem::perms::owner_exec,
                               std::filesystem::perm_options::add);
#endif

  json args = {{"path", script_path}, {"filter", json::array({{{"head", 2}}})}};
  std::string result = ai::call_tool("execute_command", args);
  EXPECT_TRUE(result.find("line1") != std::string::npos);
  EXPECT_TRUE(result.find("line2") != std::string::npos);
  EXPECT_TRUE(result.find("line3") == std::string::npos);
}

TEST(ExecuteCommandTest, FilterTail) {
  TempTestDir dir;
#if defined(_WIN32)
  std::string script_path =
      (std::filesystem::path(dir.path()) / "filter_tail.bat").string();
  {
    std::ofstream script(script_path, std::ios::binary);
    script << "@echo off\r\n"
              "echo line1\r\n"
              "echo line2\r\n"
              "echo line3\r\n"
              "echo line4\r\n"
              "echo line5\r\n";
  }
#else
  std::string script_path =
      (std::filesystem::path(dir.path()) / "filter_tail.sh").string();
  {
    std::ofstream script(script_path, std::ios::binary);
    script << "#!/bin/sh\n"
              "echo line1\n"
              "echo line2\n"
              "echo line3\n"
              "echo line4\n"
              "echo line5\n";
  }
  std::filesystem::permissions(script_path, std::filesystem::perms::owner_exec,
                               std::filesystem::perm_options::add);
#endif

  json args = {{"path", script_path}, {"filter", json::array({{{"tail", 2}}})}};
  std::string result = ai::call_tool("execute_command", args);
  EXPECT_TRUE(result.find("line1") == std::string::npos);
  EXPECT_TRUE(result.find("line4") != std::string::npos);
  EXPECT_TRUE(result.find("line5") != std::string::npos);
}

TEST(ExecuteCommandTest, FilterRegexInclude) {
  TempTestDir dir;
#if defined(_WIN32)
  std::string script_path =
      (std::filesystem::path(dir.path()) / "filter_include.bat").string();
  {
    std::ofstream script(script_path, std::ios::binary);
    script << "@echo off\r\n"
              "echo hello_world\r\n"
              "echo foo_bar\r\n"
              "echo hello_baz\r\n";
  }
#else
  std::string script_path =
      (std::filesystem::path(dir.path()) / "filter_include.sh").string();
  {
    std::ofstream script(script_path, std::ios::binary);
    script << "#!/bin/sh\n"
              "echo hello_world\n"
              "echo foo_bar\n"
              "echo hello_baz\n";
  }
  std::filesystem::permissions(script_path, std::filesystem::perms::owner_exec,
                               std::filesystem::perm_options::add);
#endif

  json args = {{"path", script_path},
               {"filter", json::array({{{"include", "hello"}}})}};
  std::string result = ai::call_tool("execute_command", args);
  EXPECT_TRUE(result.find("hello_world") != std::string::npos);
  EXPECT_TRUE(result.find("foo_bar") == std::string::npos);
  EXPECT_TRUE(result.find("hello_baz") != std::string::npos);
}

TEST(ExecuteCommandTest, FilterRegexExclude) {
  TempTestDir dir;
#if defined(_WIN32)
  std::string script_path =
      (std::filesystem::path(dir.path()) / "filter_exclude.bat").string();
  {
    std::ofstream script(script_path, std::ios::binary);
    script << "@echo off\r\n"
              "echo keep_me\r\n"
              "echo debug_info\r\n"
              "echo keep_too\r\n";
  }
#else
  std::string script_path =
      (std::filesystem::path(dir.path()) / "filter_exclude.sh").string();
  {
    std::ofstream script(script_path, std::ios::binary);
    script << "#!/bin/sh\n"
              "echo keep_me\n"
              "echo debug_info\n"
              "echo keep_too\n";
  }
  std::filesystem::permissions(script_path, std::filesystem::perms::owner_exec,
                               std::filesystem::perm_options::add);
#endif

  json args = {{"path", script_path},
               {"filter", json::array({{{"exclude", "debug"}}})}};
  std::string result = ai::call_tool("execute_command", args);
  EXPECT_TRUE(result.find("keep_me") != std::string::npos);
  EXPECT_TRUE(result.find("debug_info") == std::string::npos);
  EXPECT_TRUE(result.find("keep_too") != std::string::npos);
}

TEST(ExecuteCommandTest, FilterHeadZeroEmpty) {
  TempTestDir dir;
#if defined(_WIN32)
  std::string script_path =
      (std::filesystem::path(dir.path()) / "filter_head0.bat").string();
  {
    std::ofstream script(script_path, std::ios::binary);
    script << "@echo off\r\necho hello\r\n";
  }
#else
  std::string script_path =
      (std::filesystem::path(dir.path()) / "filter_head0.sh").string();
  {
    std::ofstream script(script_path, std::ios::binary);
    script << "#!/bin/sh\necho hello\n";
  }
  std::filesystem::permissions(script_path, std::filesystem::perms::owner_exec,
                               std::filesystem::perm_options::add);
#endif

  json args = {{"path", script_path}, {"filter", json::array({{{"head", 0}}})}};
  std::string result = ai::call_tool("execute_command", args);
  // head 0 returns 0 lines, matching Unix head -n 0 semantics
  EXPECT_EQ(result, "Exit code: 0\n(no output)");
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
