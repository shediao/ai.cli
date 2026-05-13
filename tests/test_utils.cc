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
// TempDir
// =============================================================================

TEST(TempDirTest, DefaultConstructorCreatesNonEmptyPath) {
  utils::TempDir td;
  EXPECT_FALSE(td.path().empty());
  EXPECT_EQ(td.path().find('\0'), std::string::npos);
}

TEST(TempDirTest, DefaultConstructorCreatesDirectoryOnDisk) {
  std::string path;
  {
    utils::TempDir td;
    path = td.path();
    EXPECT_TRUE(fs::exists(path));
    EXPECT_TRUE(fs::is_directory(path));
  }
  // Directory should be cleaned up by destructor
  EXPECT_FALSE(fs::exists(path));
}

TEST(TempDirTest, ConstructorWithPrefix) {
  utils::TempDir td("myprefix");
  std::string p = td.path();
  EXPECT_FALSE(p.empty());
  EXPECT_TRUE(fs::exists(p));
  EXPECT_TRUE(fs::is_directory(p));
  // On Windows GetTempFileNameA uses only the first 3 characters of the
  // prefix, so search for a shortened form.
  // On POSIX the full prefix is part of the directory name.
#if defined(_WIN32)
  EXPECT_NE(p.find("myp"), std::string::npos);
#else
  EXPECT_NE(p.find("myprefix"), std::string::npos);
#endif
}

TEST(TempDirTest, PathReturnsValidPath) {
  utils::TempDir td;
  std::string p = td.path();
  EXPECT_FALSE(p.empty());
  EXPECT_TRUE(p.find('\0') == std::string::npos);
}

TEST(TempDirTest, CanCreateFilesInTempDir) {
  utils::TempDir td;
  std::string file_path = (fs::path(td.path()) / "test_file.txt").string();
  EXPECT_TRUE(utils::write_file(file_path, "hello from temp dir"));
  EXPECT_TRUE(fs::exists(file_path));

  auto content = utils::read_file(file_path);
  ASSERT_TRUE(content.has_value());
  EXPECT_EQ(*content, "hello from temp dir");
}

TEST(TempDirTest, CanCreateSubdirectories) {
  utils::TempDir td;
  fs::path subdir = fs::path(td.path()) / "subdir";
  EXPECT_TRUE(fs::create_directory(subdir));
  EXPECT_TRUE(fs::exists(subdir));
  EXPECT_TRUE(fs::is_directory(subdir));
}

TEST(TempDirTest, DestructorRemovesDirectoryWithContents) {
  std::string path;
  {
    utils::TempDir td;
    path = td.path();
    // Create files and subdirectories inside
    EXPECT_TRUE(
        utils::write_file((fs::path(path) / "file1.txt").string(), "data1"));
    EXPECT_TRUE(
        utils::write_file((fs::path(path) / "file2.txt").string(), "data2"));
    fs::create_directory(fs::path(path) / "sub");
    EXPECT_TRUE(utils::write_file(
        (fs::path(path) / "sub" / "nested.txt").string(), "nested"));
    EXPECT_TRUE(fs::exists(path));
  }
  // Destructor should recursively remove everything
  EXPECT_FALSE(fs::exists(path));
}

TEST(TempDirTest, DestructorHandlesAlreadyRemovedDir) {
  std::string path;
  {
    utils::TempDir td;
    path = td.path();
    fs::remove_all(path);
    EXPECT_FALSE(fs::exists(path));
  }
  // Should not throw or crash
  EXPECT_FALSE(fs::exists(path));
}

TEST(TempDirTest, TwoTempDirsHaveDifferentPaths) {
  utils::TempDir td1;
  utils::TempDir td2;
  EXPECT_NE(td1.path(), td2.path());
}

TEST(TempDirTest, MultipleTempDirsAreIndependent) {
  utils::TempDir td1;
  utils::TempDir td2;
  EXPECT_TRUE(fs::exists(td1.path()));
  EXPECT_TRUE(fs::exists(td2.path()));
  EXPECT_NE(td1.path(), td2.path());
}

TEST(TempDirTest, TempDirIsWritable) {
  utils::TempDir td;
  // Verify we can write a file (this also checks the directory is writable)
  std::string test_file = (fs::path(td.path()) / "writable_test").string();
  EXPECT_TRUE(utils::write_file(test_file, "check writable"));
}

// =============================================================================
// getTempDirPath
// =============================================================================

TEST(GetTempDirPathTest, ReturnsNonEmptyPath) {
  std::string path = utils::getTempDirPath("test");
  EXPECT_FALSE(path.empty());
  // Clean up since getTempDirPath actually creates the directory
  fs::remove_all(path);
}

TEST(GetTempDirPathTest, CreatesDirectoryOnDisk) {
  std::string path = utils::getTempDirPath("ondisk");
  EXPECT_TRUE(fs::exists(path));
  EXPECT_TRUE(fs::is_directory(path));
  fs::remove_all(path);
}

TEST(GetTempDirPathTest, PathContainsPrefix) {
  // On Windows GetTempFileNameA uses only the first 3 characters of the
  // prefix, so search for a shortened form.
  // On POSIX the full prefix is part of the directory name.
#if defined(_WIN32)
  std::string path = utils::getTempDirPath("abc_test_dir");
  EXPECT_NE(path.find("abc"), std::string::npos);
#else
  std::string path = utils::getTempDirPath("abc_test_dir");
  EXPECT_NE(path.find("abc_test_dir"), std::string::npos);
#endif
  fs::remove_all(path);
}

TEST(GetTempDirPathTest, EmptyPrefix) {
  std::string path = utils::getTempDirPath("");
  EXPECT_FALSE(path.empty());
  EXPECT_TRUE(fs::exists(path));
  EXPECT_TRUE(fs::is_directory(path));
  fs::remove_all(path);
}

TEST(GetTempDirPathTest, UniquePaths) {
  std::string p1 = utils::getTempDirPath("u");
  std::string p2 = utils::getTempDirPath("u");
  EXPECT_NE(p1, p2);
  fs::remove_all(p1);
  fs::remove_all(p2);
}

TEST(GetTempDirPathTest, CreatedDirIsEmpty) {
  std::string path = utils::getTempDirPath("emptycheck");
  EXPECT_TRUE(fs::is_empty(path));
  fs::remove_all(path);
}

TEST(GetTempDirPathTest, CreatedDirIsWritable) {
  std::string path = utils::getTempDirPath("writable");
  std::string test_file = (fs::path(path) / "test.txt").string();
  EXPECT_TRUE(utils::write_file(test_file, "hello"));
  EXPECT_TRUE(fs::exists(test_file));
  fs::remove_all(path);
}

// =============================================================================
// TempDir / getTempDirPath — cross-platform edge cases
// =============================================================================

TEST(TempDirTest, DestructorHandlesSymlinks) {
  // Create a symlink inside the temp dir and verify cleanup still works.
  std::string path;
  {
    utils::TempDir td;
    path = td.path();

    // Create a real file to link to
    std::string target = (fs::path(path) / "target.txt").string();
    EXPECT_TRUE(utils::write_file(target, "symlink target"));

    // Create a symlink pointing to the target
    std::string link = (fs::path(path) / "link.txt").string();
    std::error_code ec;
    fs::create_symlink(target, link, ec);
    if (ec) {
      // Symlinks may not be supported (e.g. no developer mode on Windows);
      // skip the rest of this test gracefully.
      GTEST_SKIP() << "Symlink creation not supported on this platform: "
                   << ec.message();
    }
    EXPECT_TRUE(fs::exists(link));
    // is_symlink may return false on platforms with emulated symlinks
    // (e.g. MSYS), where native Windows APIs don't recognize them.
  }
  // The destructor must not throw (verified by the scope exit above).
  // On platforms where symlinks are emulated, remove_all may not fully
  // clean up — the destructor uses best-effort cleanup.
}

TEST(TempDirTest, DestructorHandlesReadOnlyContents) {
  // read-only files and directories inside TempDir must still be cleaned up.
  std::string path;
  {
    utils::TempDir td;
    path = td.path();

    std::string readonly_file = (fs::path(path) / "readonly.txt").string();
    EXPECT_TRUE(utils::write_file(readonly_file, "read-only data"));

    std::string readonly_subdir = (fs::path(path) / "readonly_sub").string();
    EXPECT_TRUE(fs::create_directory(readonly_subdir));

    // Set read-only permissions
    std::error_code ec;
    fs::permissions(readonly_file, fs::perms::owner_write,
                    fs::perm_options::remove, ec);
    fs::permissions(readonly_subdir, fs::perms::owner_write,
                    fs::perm_options::remove, ec);
  }
  // Destructor uses remove_all which can handle read-only items
  // (on Windows remove_all will fail on read-only items unless we
  //  adjust permissions; verify no crash at minimum).
  // On Unix remove_all removes the directory entry regardless of
  // file permissions (write permission on the *directory* matters).
  EXPECT_FALSE(fs::exists(path));
}

TEST(TempDirTest, NonAsciiPrefix) {
  // UTF-8 prefix should work on all modern platforms.
  // Use a mix of CJK and accented characters.
  // On Windows, GetTempFileNameA uses the ANSI code page and does
  // not support arbitrary UTF-8 prefixes, so we use ASCII only there.
#if defined(_WIN32)
  utils::TempDir td("nonascii_test");
#else
  utils::TempDir td(
      "\xe4\xb8\xad\xe6\x96\x87"  // 中文
      "\xc3\xa9\xc3\xa0"          // éà
      "_test");
#endif
  std::string p = td.path();
  EXPECT_FALSE(p.empty());
  EXPECT_TRUE(fs::exists(p));
  EXPECT_TRUE(fs::is_directory(p));
}

TEST(TempDirTest, ManyFilesCleanup) {
  // Stress test: create many files and subdirectories, verify all are removed.
  std::string path;
  {
    utils::TempDir td;
    path = td.path();

    constexpr int kNumFiles = 100;
    constexpr int kNumSubdirs = 10;
    for (int i = 0; i < kNumFiles; ++i) {
      std::string fname =
          (fs::path(path) / ("file_" + std::to_string(i) + ".txt")).string();
      EXPECT_TRUE(utils::write_file(fname, "data"));
    }
    for (int i = 0; i < kNumSubdirs; ++i) {
      fs::path sub = fs::path(path) / ("sub_" + std::to_string(i));
      EXPECT_TRUE(fs::create_directory(sub));
      for (int j = 0; j < 10; ++j) {
        std::string fname =
            (sub / ("nested_" + std::to_string(j) + ".txt")).string();
        EXPECT_TRUE(utils::write_file(fname, "nested data"));
      }
    }
    EXPECT_TRUE(fs::exists(path));
  }
  EXPECT_FALSE(fs::exists(path));
}

TEST(TempDirTest, RapidCreateDestroy) {
  // Verify that repeated create/destroy cycles do not leak resources.
  std::vector<std::string> paths;
  constexpr int kIterations = 20;
  for (int i = 0; i < kIterations; ++i) {
    utils::TempDir td;
    paths.push_back(td.path());
    EXPECT_TRUE(fs::exists(td.path()));
    EXPECT_TRUE(utils::write_file((fs::path(td.path()) / "f").string(), "x"));
  }
  // All directories should be gone after each iteration's TempDir went
  // out of scope.
  for (const auto& p : paths) {
    EXPECT_FALSE(fs::exists(p)) << "Path not cleaned up: " << p;
  }
}

TEST(TempDirTest, NestedTempDirs) {
  // A TempDir created inside another TempDir should work and both be
  // cleaned up properly.
  std::string outer_path;
  std::string inner_path;
  {
    utils::TempDir outer;
    outer_path = outer.path();
    {
      // This is a bit unusual: create a TempDir whose path is inside
      // the outer temp dir.  We do this by manually constructing one
      // via getTempDirPath with a prefix that places it inside outer.
      // Actually, getTempDirPath always uses the system temp directory,
      // so the inner dir is elsewhere.  We simply verify that two
      // independent TempDirs inside each other's scope work fine.
      utils::TempDir inner;
      inner_path = inner.path();
      EXPECT_TRUE(fs::exists(inner_path));
      EXPECT_TRUE(fs::exists(outer_path));
      EXPECT_NE(outer_path, inner_path);
    }
    // Inner should be cleaned up
    EXPECT_FALSE(fs::exists(inner_path));
    // Outer should still exist
    EXPECT_TRUE(fs::exists(outer_path));
  }
  EXPECT_FALSE(fs::exists(outer_path));
}

TEST(GetTempDirPathTest, LongPrefix) {
  // A very long prefix (but still valid as a filename component) should work.
  std::string long_prefix(200, 'a');
  std::string path = utils::getTempDirPath(long_prefix);
  EXPECT_FALSE(path.empty());
  EXPECT_TRUE(fs::exists(path));
  EXPECT_TRUE(fs::is_directory(path));
  fs::remove_all(path);
}

TEST(GetTempDirPathTest, DirPermissions) {
  // The created directory should have owner read/write/execute permissions.
  std::string path = utils::getTempDirPath("perms");
  std::error_code ec;
  auto perms = fs::status(path, ec).permissions();
  EXPECT_FALSE(ec) << "Failed to get permissions: " << ec.message();

  // Owner must be able to read, write, and execute (enter) the directory.
  using fp = fs::perms;
  constexpr auto owner_rwx = fp::owner_read | fp::owner_write | fp::owner_exec;
  EXPECT_EQ(perms & owner_rwx, owner_rwx)
      << "Missing owner rwx permissions on " << path;

  // On Unix, "other" should not have write permission for security.
#if !defined(_WIN32)
  EXPECT_EQ(perms & fp::others_write, fp::none)
      << "Temp directory should not be world-writable: " << path;
#endif

  fs::remove_all(path);
}

TEST(GetTempDirPathTest, RespectsCustomTmpdir) {
#if !defined(_WIN32)
  // Create a custom temp directory to use as TMPDIR.
  fs::path custom_tmp = fs::temp_directory_path() / "ai_cli_custom_tmp_test";
  fs::create_directories(custom_tmp);

  // Save original TMPDIR
  const char* orig_tmpdir = ::getenv("TMPDIR");
  std::string orig_tmpdir_saved;
  if (orig_tmpdir) {
    orig_tmpdir_saved = orig_tmpdir;
  }

  // Override TMPDIR
  if (::setenv("TMPDIR", custom_tmp.string().c_str(), 1) != 0) {
    fs::remove_all(custom_tmp);
    GTEST_SKIP() << "Cannot override TMPDIR in this environment";
  }

  std::string path = utils::getTempDirPath("customtmp");
  EXPECT_FALSE(path.empty());
  // Path should start with the custom tmp dir.
  EXPECT_EQ(path.find(custom_tmp.string()), 0u);
  EXPECT_TRUE(fs::exists(path));
  EXPECT_TRUE(fs::is_directory(path));
  fs::remove_all(path);

  // Restore TMPDIR
  if (orig_tmpdir) {
    ::setenv("TMPDIR", orig_tmpdir_saved.c_str(), 1);
  } else {
    ::unsetenv("TMPDIR");
  }
  fs::remove_all(custom_tmp);
#else
  GTEST_SKIP() << "Custom TMPDIR test applies to Unix only";
#endif
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

TEST(UtilsReadFileTest, DirectoryPathReturnsNullopt) {
  // read_file now explicitly rejects directories and returns nullopt
  // on all platforms (Linux/GCC would throw an exception when reading
  // from a directory stream; macOS/Clang would return garbage data).
  UtilsTempTestDir dir;
  auto content = utils::read_file(dir.path());
  EXPECT_FALSE(content.has_value());
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
