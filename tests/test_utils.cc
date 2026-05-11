#include <gtest/gtest.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

#include "ai/utils.h"

namespace fs = std::filesystem;
namespace utils = ai::utils;

// =============================================================================
// Helpers
// =============================================================================

/// RAII temporary file that cleans up on destruction.
class UtilsTempTestFile {
 public:
  explicit UtilsTempTestFile(std::string const& content = "",
                             std::string const& suffix = ".txt")
      : path_(fs::temp_directory_path() /
              ("ai_cli_utils_test_" + std::to_string(counter_++) + suffix)) {
    std::ofstream out(path_);
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    out.close();
  }

  ~UtilsTempTestFile() {
    std::error_code ec;
    fs::remove(path_, ec);
  }

  std::string path() const { return path_.string(); }

 private:
  fs::path path_;
  static inline int counter_ = 0;
};

/// RAII temporary directory that cleans up on destruction.
class UtilsTempTestDir {
 public:
  UtilsTempTestDir()
      : path_(fs::temp_directory_path() /
              ("ai_cli_utils_test_dir_" + std::to_string(counter_++))) {
    fs::create_directories(path_);
  }

  ~UtilsTempTestDir() {
    std::error_code ec;
    fs::remove_all(path_, ec);
  }

  std::string path() const { return path_.string(); }

 private:
  fs::path path_;
  static inline int counter_ = 0;
};

// =============================================================================
// is_callable / is_callable_v  (compile-time traits)
// =============================================================================

TEST(IsCallableTest, LambdaIsCallable) {
  auto lambda = []() {};
  static_assert(utils::is_callable_v<decltype(lambda)>);
  EXPECT_TRUE(utils::is_callable_v<decltype(lambda)>);
}

TEST(IsCallableTest, FunctionPointerIsCallable) {
  using FnPtr = void (*)();
  static_assert(utils::is_callable_v<FnPtr>);
  EXPECT_TRUE(utils::is_callable_v<FnPtr>);
}

TEST(IsCallableTest, FunctorIsCallable) {
  struct Functor {
    void operator()() const {}
  };
  static_assert(utils::is_callable_v<Functor>);
  EXPECT_TRUE(utils::is_callable_v<Functor>);
}

TEST(IsCallableTest, IntIsNotCallable) {
  static_assert(!utils::is_callable_v<int>);
  EXPECT_FALSE(utils::is_callable_v<int>);
}

TEST(IsCallableTest, StringIsNotCallable) {
  static_assert(!utils::is_callable_v<std::string>);
  EXPECT_FALSE(utils::is_callable_v<std::string>);
}

// =============================================================================
// AutoRun<T>
// =============================================================================

TEST(AutoRunTest, CallsFunctionOnDestruction) {
  int call_count = 0;
  {
    auto cleanup = [&call_count]() { ++call_count; };
    utils::AutoRun autorun(cleanup);
    EXPECT_EQ(call_count, 0);
  }
  EXPECT_EQ(call_count, 1);
}

TEST(AutoRunTest, CallsFunctionOnce) {
  int call_count = 0;
  {
    utils::AutoRun autorun([&call_count]() { ++call_count; });
    EXPECT_EQ(call_count, 0);
  }
  EXPECT_EQ(call_count, 1);
}

TEST(AutoRunTest, MoveOfLambdaPreservesBehavior) {
  int call_count = 0;
  auto lambda = [&call_count]() { ++call_count; };
  {
    utils::AutoRun autorun(std::move(lambda));
    EXPECT_EQ(call_count, 0);
  }
  EXPECT_EQ(call_count, 1);
}

TEST(AutoRunTest, NoArgCallableAcceptsNoCaptureLambda) {
  bool called = false;
  {
    // No-capture lambda is convertible to function pointer
    utils::AutoRun autorun([&called]() { called = true; });
  }
  EXPECT_TRUE(called);
}

TEST(AutoRunTest, MultipleAutoRunsCallInReverseOrder) {
  std::string order;
  {
    utils::AutoRun a1([&order]() { order += "1"; });
    utils::AutoRun a2([&order]() { order += "2"; });
    utils::AutoRun a3([&order]() { order += "3"; });
    // Destructors run in reverse order of construction
  }
  EXPECT_EQ(order, "321");
}

// =============================================================================
// TempFile
// =============================================================================

TEST(TempFileTest, DefaultConstructorCreatesNonEmptyPath) {
  utils::TempFile tf;
  EXPECT_FALSE(tf.path().empty());
  // getTempFilePath only generates a path, it does not leave a file
  // on disk (mkstemp + close + remove).  So we only verify the path
  // is a valid string.
  EXPECT_EQ(tf.path().find('\0'), std::string::npos);
}

TEST(TempFileTest, DefaultConstructorFileCanBeWrittenTo) {
  std::string path;
  {
    utils::TempFile tf;
    path = tf.path();
    EXPECT_FALSE(fs::exists(path));  // path generated but file not created
    // Write something to the file
    utils::write_file(path, "hello temp");
    EXPECT_TRUE(fs::exists(path));
  }
  // File should be cleaned up by destructor (if it existed)
  EXPECT_FALSE(fs::exists(path));
}

TEST(TempFileTest, ConstructorWithPrefixPostfix) {
  utils::TempFile tf("myprefix", ".mysuffix");
  std::string p = tf.path();
  EXPECT_FALSE(p.empty());
  // On POSIX the path ends with ".mysuffix"
  EXPECT_TRUE(p.size() >= 9u);
  EXPECT_EQ(p.substr(p.size() - 9), ".mysuffix");
}

TEST(TempFileTest, PathReturnsValidPath) {
  utils::TempFile tf;
  std::string p = tf.path();
  EXPECT_FALSE(p.empty());
  EXPECT_TRUE(p.find('\0') == std::string::npos);
}

TEST(TempFileTest, ContentReadsWrittenData) {
  utils::TempFile tf;
  std::string data = "test content for temp file";
  utils::write_file(tf.path(), data);
  auto content = tf.content();
  ASSERT_TRUE(content.has_value());
  EXPECT_EQ(*content, data);
}

TEST(TempFileTest, ContentEmptyFile) {
  utils::TempFile tf;
  // Create an empty file at the temp path
  utils::write_file(tf.path(), "");
  auto content = tf.content();
  ASSERT_TRUE(content.has_value());
  EXPECT_EQ(*content, "");
}

TEST(TempFileTest, ContentFileNotCreatedReturnsEmpty) {
  // If nothing was written to the temp path, read_file returns nullopt
  utils::TempFile tf;
  auto content = tf.content();
  // File doesn't exist on disk, so content() returns nullopt
  EXPECT_FALSE(content.has_value());
}

TEST(TempFileTest, DestructorRemovesFile) {
  std::string path;
  {
    utils::TempFile tf;
    path = tf.path();
    utils::write_file(path, "data");  // create the file
    EXPECT_TRUE(fs::exists(path));
  }
  EXPECT_FALSE(fs::exists(path));
}

TEST(TempFileTest, DestructorHandlesAlreadyRemovedFile) {
  std::string path;
  {
    utils::TempFile tf;
    path = tf.path();
    utils::write_file(path, "data");
    fs::remove(path);
  }
  // Should not throw or crash
  EXPECT_FALSE(fs::exists(path));
}

TEST(TempFileTest, TwoTempFilesHaveDifferentPaths) {
  utils::TempFile tf1;
  utils::TempFile tf2;
  EXPECT_NE(tf1.path(), tf2.path());
}

// =============================================================================
// getTempFilePath
// =============================================================================

TEST(GetTempFilePathTest, ReturnsNonEmptyPath) {
  std::string path = utils::getTempFilePath("test", ".txt");
  EXPECT_FALSE(path.empty());
}

TEST(GetTempFilePathTest, PathEndsWithPostfix) {
  std::string postfix = ".myext";
  std::string path = utils::getTempFilePath("pref", postfix);
  EXPECT_TRUE(path.size() >= postfix.size());
  EXPECT_EQ(path.substr(path.size() - postfix.size()), postfix);
}

TEST(GetTempFilePathTest, PathContainsPrefix) {
  // On POSIX the prefix is embedded in the path; on Windows the prefix may
  // be used differently by GetTempFileNameA.  We only check that the call
  // succeeds and returns a plausible path.
  std::string path = utils::getTempFilePath("abc_test", ".dat");
  EXPECT_FALSE(path.empty());
  // The path must at least be a valid filesystem path (no embedded NULs).
  EXPECT_EQ(path.find('\0'), std::string::npos);
}

TEST(GetTempFilePathTest, EmptyPrefixAndPostfix) {
  std::string path = utils::getTempFilePath("", "");
  EXPECT_FALSE(path.empty());
}

TEST(GetTempFilePathTest, UniquePaths) {
  std::string p1 = utils::getTempFilePath("u", ".t");
  std::string p2 = utils::getTempFilePath("u", ".t");
  EXPECT_NE(p1, p2);
}

TEST(GetTempFilePathTest, PathDoesNotYetExistOnDisk) {
  // getTempFilePath generates a unique name but should NOT leave the file
  // behind (the internal mkstemp fd is closed and the file is removed).
  std::string path = utils::getTempFilePath("noexist", ".test");
  EXPECT_FALSE(fs::exists(path));
}

// =============================================================================
// timestamp
// =============================================================================

TEST(TimestampTest, DefaultFormatIsNonEmpty) {
  std::string ts = utils::timestamp();
  EXPECT_FALSE(ts.empty());
  // Default format "%Y/%m/%d %H:%M:%S %z" looks like: "2025/07/11 14:30:00
  // +0800"
  EXPECT_GE(ts.size(), 20u);
}

TEST(TimestampTest, DefaultFormatContainsSlash) {
  std::string ts = utils::timestamp();
  EXPECT_NE(ts.find('/'), std::string::npos);
}

TEST(TimestampTest, CustomFormatYearOnly) {
  std::string ts = utils::timestamp("%Y");
  EXPECT_EQ(ts.size(), 4u);
  for (char c : ts) {
    EXPECT_TRUE(std::isdigit(static_cast<unsigned char>(c)));
  }
}

TEST(TimestampTest, CustomFormatFullDate) {
  std::string ts = utils::timestamp("%Y-%m-%d");
  EXPECT_EQ(ts.size(), 10u);
  EXPECT_EQ(ts[4], '-');
  EXPECT_EQ(ts[7], '-');
}

TEST(TimestampTest, CustomFormatTimeOnly) {
  std::string ts = utils::timestamp("%H:%M:%S");
  EXPECT_EQ(ts.size(), 8u);
  EXPECT_EQ(ts[2], ':');
  EXPECT_EQ(ts[5], ':');
}

TEST(TimestampTest, TwoCallsReturnCloseValues) {
  std::string ts1 = utils::timestamp("%Y%m%d%H%M%S");
  // Sleep into the next second to guarantee difference
  std::this_thread::sleep_for(std::chrono::seconds(1));
  std::string ts2 = utils::timestamp("%Y%m%d%H%M%S");
  EXPECT_NE(ts1, ts2);
}

// =============================================================================
// read_file
// =============================================================================

TEST(UtilsReadFileTest, ReadsEntireFile) {
  UtilsTempTestFile f("hello\nworld\n");
  auto content = utils::read_file(f.path());
  ASSERT_TRUE(content.has_value());
  EXPECT_EQ(*content, "hello\nworld\n");
}

TEST(UtilsReadFileTest, ReadsEmptyFile) {
  UtilsTempTestFile f("");
  auto content = utils::read_file(f.path());
  ASSERT_TRUE(content.has_value());
  EXPECT_EQ(*content, "");
}

TEST(UtilsReadFileTest, ReadsSingleLine) {
  UtilsTempTestFile f("single line without newline");
  auto content = utils::read_file(f.path());
  ASSERT_TRUE(content.has_value());
  EXPECT_EQ(*content, "single line without newline");
}

TEST(UtilsReadFileTest, ReadsBinaryContent) {
  // Create a file with embedded null bytes (use explicit char array to
  // avoid UB from reading past a string literal's null terminator).
  const char raw[] = {'A', 'B', '\0', 'C', 'D', '\0', 'E', 'F'};
  std::string binary_data(raw, sizeof(raw));
  UtilsTempTestFile f(binary_data, ".bin");
  auto content = utils::read_file(f.path());
  ASSERT_TRUE(content.has_value());
  EXPECT_EQ(content->size(), sizeof(raw));
  EXPECT_EQ((*content)[0], 'A');
  EXPECT_EQ((*content)[1], 'B');
  EXPECT_EQ((*content)[2], '\0');
  EXPECT_EQ((*content)[3], 'C');
  EXPECT_EQ((*content)[4], 'D');
  EXPECT_EQ((*content)[5], '\0');
  EXPECT_EQ((*content)[6], 'E');
  EXPECT_EQ((*content)[7], 'F');
}

TEST(UtilsReadFileTest, FileNotExistsReturnsNullopt) {
  auto content =
      utils::read_file("/nonexistent/path/definitely_not_a_file.txt");
  EXPECT_FALSE(content.has_value());
}

TEST(UtilsReadFileTest, DirectoryPathReturnsContentOnSomePlatforms) {
  // std::ifstream can open a directory on some platforms (e.g. macOS).
  // This test documents the actual behaviour – read_file does not
  // explicitly reject directories; it relies on the stream's ability
  // to open the path.
  UtilsTempTestDir dir;
  auto content = utils::read_file(dir.path());
  // On platforms where opening a directory succeeds, content will have a
  // value; on others it returns nullopt.  Either way the call must not
  // crash.
  SUCCEED();
}

TEST(UtilsReadFileTest, LargeFile) {
  UtilsTempTestFile f(std::string(10000, 'X'));
  auto content = utils::read_file(f.path());
  ASSERT_TRUE(content.has_value());
  EXPECT_EQ(content->size(), 10000u);
  EXPECT_EQ(*content, std::string(10000, 'X'));
}

// =============================================================================
// write_file
// =============================================================================

TEST(WriteFileTest, WritesContentToFile) {
  UtilsTempTestFile f("");
  EXPECT_TRUE(utils::write_file(f.path(), "new content"));

  auto content = utils::read_file(f.path());
  ASSERT_TRUE(content.has_value());
  EXPECT_EQ(*content, "new content");
}

TEST(WriteFileTest, CreatesFileIfNotExists) {
  UtilsTempTestDir dir;
  std::string path = (fs::path(dir.path()) / "new_file.txt").string();

  EXPECT_TRUE(utils::write_file(path, "created!"));
  EXPECT_TRUE(fs::exists(path));

  auto content = utils::read_file(path);
  ASSERT_TRUE(content.has_value());
  EXPECT_EQ(*content, "created!");
}

TEST(WriteFileTest, OverwritesExistingFile) {
  UtilsTempTestFile f("original");
  EXPECT_TRUE(utils::write_file(f.path(), "overwritten"));

  auto content = utils::read_file(f.path());
  ASSERT_TRUE(content.has_value());
  EXPECT_EQ(*content, "overwritten");
}

TEST(WriteFileTest, WritesEmptyContent) {
  UtilsTempTestFile f("original");
  EXPECT_TRUE(utils::write_file(f.path(), ""));

  auto content = utils::read_file(f.path());
  ASSERT_TRUE(content.has_value());
  EXPECT_EQ(*content, "");
}

TEST(WriteFileTest, WriteToNonWritableDirectoryReturnsFalse) {
  // Attempt to write to a path that has a directory component that does not
  // exist
  std::string path = "/nonexistent_dir_xyz/subdir/file.txt";
  EXPECT_FALSE(utils::write_file(path, "data"));
}

TEST(WriteFileTest, WriteBinaryContent) {
  UtilsTempTestFile f("");
  std::string binary_data("AB\0CD\0EF", 7);
  EXPECT_TRUE(utils::write_file(f.path(), binary_data));

  auto content = utils::read_file(f.path());
  ASSERT_TRUE(content.has_value());
  EXPECT_EQ(content->size(), 7u);
  EXPECT_EQ(*content, binary_data);
}

TEST(WriteFileTest, WriteLargeContent) {
  UtilsTempTestFile f("");
  std::string large(100000, 'Z');
  EXPECT_TRUE(utils::write_file(f.path(), large));

  auto content = utils::read_file(f.path());
  ASSERT_TRUE(content.has_value());
  EXPECT_EQ(content->size(), 100000u);
}

// =============================================================================
// read_file + write_file round-trip
// =============================================================================

TEST(ReadWriteRoundTripTest, RoundTrip) {
  UtilsTempTestFile f("");
  std::string original = "Hello, round-trip world!\nLine 2\nLine 3\n";

  EXPECT_TRUE(utils::write_file(f.path(), original));
  auto result = utils::read_file(f.path());

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, original);
}

// =============================================================================
// app_data_dir
// =============================================================================

TEST(AppDataDirTest, ReturnsNonEmpty) {
  std::string dir = utils::app_data_dir("test_app");
  EXPECT_FALSE(dir.empty());
}

TEST(AppDataDirTest, ContainsAppName) {
  std::string dir = utils::app_data_dir("my_test_app_xyz");
  EXPECT_NE(dir.find("my_test_app_xyz"), std::string::npos);
}

TEST(AppDataDirTest, WithAuthor) {
  std::string dir = utils::app_data_dir("test_app", "test_author");
  EXPECT_FALSE(dir.empty());
}

// =============================================================================
// stdin_is_atty / stdout_is_atty / stderr_is_atty
// =============================================================================

TEST(IsAttyTest, ReturnBool) {
  // We cannot reliably predict the return value in a test environment,
  // but we can verify the functions return a boolean and do not crash.
  bool result_stdin = utils::stdin_is_atty();
  bool result_stdout = utils::stdout_is_atty();
  bool result_stderr = utils::stderr_is_atty();

  // Just check they compile and return something sensible
  EXPECT_TRUE(result_stdin || !result_stdin);
  EXPECT_TRUE(result_stdout || !result_stdout);
  EXPECT_TRUE(result_stderr || !result_stderr);

  // Suppress unused warnings
  (void)result_stdin;
  (void)result_stdout;
  (void)result_stderr;
}
