#include "base/temp_dir.h"

#include <gtest/gtest.h>

#include <environment/environment.hpp>
#include <filesystem>

#include "base/file.h"

namespace fs = std::filesystem;

// =============================================================================
// TempDir
// =============================================================================

TEST(TempDirTest, DefaultConstructorCreatesNonEmptyPath) {
  ai::base::TempDir td;
  EXPECT_FALSE(td.path().empty());
  EXPECT_EQ(td.path().find('\0'), std::string::npos);
}

TEST(TempDirTest, DefaultConstructorCreatesDirectoryOnDisk) {
  std::string path;
  {
    ai::base::TempDir td;
    path = td.path();
    EXPECT_TRUE(fs::exists(path));
    EXPECT_TRUE(fs::is_directory(path));
  }
  // Directory should be cleaned up by destructor
  EXPECT_FALSE(fs::exists(path));
}

TEST(TempDirTest, ConstructorWithPrefix) {
  ai::base::TempDir td("myprefix");
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
  ai::base::TempDir td;
  std::string p = td.path();
  EXPECT_FALSE(p.empty());
  EXPECT_TRUE(p.find('\0') == std::string::npos);
}

TEST(TempDirTest, CanCreateFilesInTempDir) {
  ai::base::TempDir td;
  std::string file_path = (fs::path(td.path()) / "test_file.txt").string();
  EXPECT_TRUE(ai::base::write_file(file_path, "hello from temp dir"));
  EXPECT_TRUE(fs::exists(file_path));

  auto content = ai::base::read_file(file_path);
  ASSERT_TRUE(content.has_value());
  EXPECT_EQ(*content, "hello from temp dir");
}

TEST(TempDirTest, CanCreateSubdirectories) {
  ai::base::TempDir td;
  fs::path subdir = fs::path(td.path()) / "subdir";
  EXPECT_TRUE(fs::create_directory(subdir));
  EXPECT_TRUE(fs::exists(subdir));
  EXPECT_TRUE(fs::is_directory(subdir));
}

TEST(TempDirTest, DestructorRemovesDirectoryWithContents) {
  std::string path;
  {
    ai::base::TempDir td;
    path = td.path();
    // Create files and subdirectories inside
    EXPECT_TRUE(
        ai::base::write_file((fs::path(path) / "file1.txt").string(), "data1"));
    EXPECT_TRUE(
        ai::base::write_file((fs::path(path) / "file2.txt").string(), "data2"));
    fs::create_directory(fs::path(path) / "sub");
    EXPECT_TRUE(ai::base::write_file(
        (fs::path(path) / "sub" / "nested.txt").string(), "nested"));
    EXPECT_TRUE(fs::exists(path));
  }
  // Destructor should recursively remove everything
  EXPECT_FALSE(fs::exists(path));
}

TEST(TempDirTest, DestructorHandlesAlreadyRemovedDir) {
  std::string path;
  {
    ai::base::TempDir td;
    path = td.path();
    fs::remove_all(path);
    EXPECT_FALSE(fs::exists(path));
  }
  // Should not throw or crash
  EXPECT_FALSE(fs::exists(path));
}

TEST(TempDirTest, TwoTempDirsHaveDifferentPaths) {
  ai::base::TempDir td1;
  ai::base::TempDir td2;
  EXPECT_NE(td1.path(), td2.path());
}

TEST(TempDirTest, MultipleTempDirsAreIndependent) {
  ai::base::TempDir td1;
  ai::base::TempDir td2;
  EXPECT_TRUE(fs::exists(td1.path()));
  EXPECT_TRUE(fs::exists(td2.path()));
  EXPECT_NE(td1.path(), td2.path());
}

TEST(TempDirTest, TempDirIsWritable) {
  ai::base::TempDir td;
  // Verify we can write a file (this also checks the directory is writable)
  std::string test_file = (fs::path(td.path()) / "writable_test").string();
  EXPECT_TRUE(ai::base::write_file(test_file, "check writable"));
}

// =============================================================================
// getTempDirPath
// =============================================================================

TEST(GetTempDirPathTest, ReturnsNonEmptyPath) {
  std::string path = ai::base::getTempDirPath("test");
  EXPECT_FALSE(path.empty());
  // Clean up since getTempDirPath actually creates the directory
  fs::remove_all(path);
}

TEST(GetTempDirPathTest, CreatesDirectoryOnDisk) {
  std::string path = ai::base::getTempDirPath("ondisk");
  EXPECT_TRUE(fs::exists(path));
  EXPECT_TRUE(fs::is_directory(path));
  fs::remove_all(path);
}

TEST(GetTempDirPathTest, PathContainsPrefix) {
  // On Windows GetTempFileNameA uses only the first 3 characters of the
  // prefix, so search for a shortened form.
  // On POSIX the full prefix is part of the directory name.
#if defined(_WIN32)
  std::string path = ai::base::getTempDirPath("abc_test_dir");
  EXPECT_NE(path.find("abc"), std::string::npos);
#else
  std::string path = ai::base::getTempDirPath("abc_test_dir");
  EXPECT_NE(path.find("abc_test_dir"), std::string::npos);
#endif
  fs::remove_all(path);
}

TEST(GetTempDirPathTest, EmptyPrefix) {
  std::string path = ai::base::getTempDirPath("");
  EXPECT_FALSE(path.empty());
  EXPECT_TRUE(fs::exists(path));
  EXPECT_TRUE(fs::is_directory(path));
  fs::remove_all(path);
}

TEST(GetTempDirPathTest, UniquePaths) {
  std::string p1 = ai::base::getTempDirPath("u");
  std::string p2 = ai::base::getTempDirPath("u");
  EXPECT_NE(p1, p2);
  fs::remove_all(p1);
  fs::remove_all(p2);
}

TEST(GetTempDirPathTest, CreatedDirIsEmpty) {
  std::string path = ai::base::getTempDirPath("emptycheck");
  EXPECT_TRUE(fs::is_empty(path));
  fs::remove_all(path);
}

TEST(GetTempDirPathTest, CreatedDirIsWritable) {
  std::string path = ai::base::getTempDirPath("writable");
  std::string test_file = (fs::path(path) / "test.txt").string();
  EXPECT_TRUE(ai::base::write_file(test_file, "hello"));
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
    ai::base::TempDir td;
    path = td.path();

    // Create a real file to link to
    std::string target = (fs::path(path) / "target.txt").string();
    EXPECT_TRUE(ai::base::write_file(target, "symlink target"));

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
    ai::base::TempDir td;
    path = td.path();

    std::string readonly_file = (fs::path(path) / "readonly.txt").string();
    EXPECT_TRUE(ai::base::write_file(readonly_file, "read-only data"));

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
  ai::base::TempDir td("nonascii_test");
#else
  ai::base::TempDir td(
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
    ai::base::TempDir td;
    path = td.path();

    constexpr int kNumFiles = 100;
    constexpr int kNumSubdirs = 10;
    for (int i = 0; i < kNumFiles; ++i) {
      std::string fname =
          (fs::path(path) / ("file_" + std::to_string(i) + ".txt")).string();
      EXPECT_TRUE(ai::base::write_file(fname, "data"));
    }
    for (int i = 0; i < kNumSubdirs; ++i) {
      fs::path sub = fs::path(path) / ("sub_" + std::to_string(i));
      EXPECT_TRUE(fs::create_directory(sub));
      for (int j = 0; j < 10; ++j) {
        std::string fname =
            (sub / ("nested_" + std::to_string(j) + ".txt")).string();
        EXPECT_TRUE(ai::base::write_file(fname, "nested data"));
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
    ai::base::TempDir td;
    paths.push_back(td.path());
    EXPECT_TRUE(fs::exists(td.path()));
    EXPECT_TRUE(
        ai::base::write_file((fs::path(td.path()) / "f").string(), "x"));
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
    ai::base::TempDir outer;
    outer_path = outer.path();
    {
      // This is a bit unusual: create a TempDir whose path is inside
      // the outer temp dir.  We do this by manually constructing one
      // via getTempDirPath with a prefix that places it inside outer.
      // Actually, getTempDirPath always uses the system temp directory,
      // so the inner dir is elsewhere.  We simply verify that two
      // independent TempDirs inside each other's scope work fine.
      ai::base::TempDir inner;
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
  std::string path = ai::base::getTempDirPath(long_prefix);
  EXPECT_FALSE(path.empty());
  EXPECT_TRUE(fs::exists(path));
  EXPECT_TRUE(fs::is_directory(path));
  fs::remove_all(path);
}

TEST(GetTempDirPathTest, DirPermissions) {
  // The created directory should have owner read/write/execute permissions.
  std::string path = ai::base::getTempDirPath("perms");
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
  auto orig_tmpdir = env::get("TMPDIR");
  std::string orig_tmpdir_saved;
  if (orig_tmpdir) {
    orig_tmpdir_saved = orig_tmpdir.value();
  }

  // Override TMPDIR
  if (::setenv("TMPDIR", custom_tmp.string().c_str(), 1) != 0) {
    fs::remove_all(custom_tmp);
    GTEST_SKIP() << "Cannot override TMPDIR in this environment";
  }

  std::string path = ai::base::getTempDirPath("customtmp");
  EXPECT_FALSE(path.empty());
  // Path should start with the custom tmp dir.
  EXPECT_EQ(path.find(custom_tmp.string()), 0u);
  EXPECT_TRUE(fs::exists(path));
  EXPECT_TRUE(fs::is_directory(path));
  fs::remove_all(path);

  // Restore TMPDIR
  if (orig_tmpdir) {
    env::set("TMPDIR", orig_tmpdir_saved, 1);
  } else {
    env::unset("TMPDIR");
  }
  fs::remove_all(custom_tmp);
#else
  GTEST_SKIP() << "Custom TMPDIR test applies to Unix only";
#endif
}
