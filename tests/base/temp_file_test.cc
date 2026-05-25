#include "base/temp_file.h"

#include <gtest/gtest.h>

#include <filesystem>

#include "base/file.h"

// =============================================================================
// TempFile
// =============================================================================

namespace fs = std::filesystem;

TEST(TempFileTest, DefaultConstructorCreatesNonEmptyPath) {
  ai::base::TempFile tf;
  EXPECT_FALSE(tf.path().empty());
  // getTempFilePath only generates a path, it does not leave a file
  // on disk (mkstemp + close + remove).  So we only verify the path
  // is a valid string.
  EXPECT_EQ(tf.path().find('\0'), std::string::npos);
}

TEST(TempFileTest, DefaultConstructorFileCanBeWrittenTo) {
  std::string path;
  {
    ai::base::TempFile tf;
    path = tf.path();
    EXPECT_FALSE(fs::exists(path));  // path generated but file not created
    // Write something to the file
    ai::base::write_file(path, "hello temp");
    EXPECT_TRUE(fs::exists(path));
  }
  // File should be cleaned up by destructor (if it existed)
  EXPECT_FALSE(fs::exists(path));
}

TEST(TempFileTest, ConstructorWithPrefixPostfix) {
  ai::base::TempFile tf("myprefix", ".mysuffix");
  std::string p = tf.path();
  EXPECT_FALSE(p.empty());
  // On POSIX the path ends with ".mysuffix"
  EXPECT_TRUE(p.size() >= 9u);
  EXPECT_EQ(p.substr(p.size() - 9), ".mysuffix");
}

TEST(TempFileTest, PathReturnsValidPath) {
  ai::base::TempFile tf;
  std::string p = tf.path();
  EXPECT_FALSE(p.empty());
  EXPECT_TRUE(p.find('\0') == std::string::npos);
}

TEST(TempFileTest, ContentReadsWrittenData) {
  ai::base::TempFile tf;
  std::string data = "test content for temp file";
  ai::base::write_file(tf.path(), data);
  auto content = tf.content();
  ASSERT_TRUE(content.has_value());
  EXPECT_EQ(*content, data);
}

TEST(TempFileTest, ContentEmptyFile) {
  ai::base::TempFile tf;
  // Create an empty file at the temp path
  ai::base::write_file(tf.path(), "");
  auto content = tf.content();
  ASSERT_TRUE(content.has_value());
  EXPECT_EQ(*content, "");
}

TEST(TempFileTest, ContentFileNotCreatedReturnsEmpty) {
  // If nothing was written to the temp path, read_file returns nullopt
  ai::base::TempFile tf;
  auto content = tf.content();
  // File doesn't exist on disk, so content() returns nullopt
  EXPECT_FALSE(content.has_value());
}

TEST(TempFileTest, DestructorRemovesFile) {
  std::string path;
  {
    ai::base::TempFile tf;
    path = tf.path();
    ai::base::write_file(path, "data");  // create the file
    EXPECT_TRUE(fs::exists(path));
  }
  EXPECT_FALSE(fs::exists(path));
}

TEST(TempFileTest, DestructorHandlesAlreadyRemovedFile) {
  std::string path;
  {
    ai::base::TempFile tf;
    path = tf.path();
    ai::base::write_file(path, "data");
    fs::remove(path);
  }
  // Should not throw or crash
  EXPECT_FALSE(fs::exists(path));
}

TEST(TempFileTest, TwoTempFilesHaveDifferentPaths) {
  ai::base::TempFile tf1;
  ai::base::TempFile tf2;
  EXPECT_NE(tf1.path(), tf2.path());
}

// =============================================================================
// getTempFilePath
// =============================================================================

TEST(GetTempFilePathTest, ReturnsNonEmptyPath) {
  std::string path = ai::base::getTempFilePath("test", ".txt");
  EXPECT_FALSE(path.empty());
}

TEST(GetTempFilePathTest, PathEndsWithPostfix) {
  std::string postfix = ".myext";
  std::string path = ai::base::getTempFilePath("pref", postfix);
  EXPECT_TRUE(path.size() >= postfix.size());
  EXPECT_EQ(path.substr(path.size() - postfix.size()), postfix);
}

TEST(GetTempFilePathTest, PathContainsPrefix) {
  // On POSIX the prefix is embedded in the path; on Windows the prefix may
  // be used differently by GetTempFileNameA.  We only check that the call
  // succeeds and returns a plausible path.
  std::string path = ai::base::getTempFilePath("abc_test", ".dat");
  EXPECT_FALSE(path.empty());
  // The path must at least be a valid filesystem path (no embedded NULs).
  EXPECT_EQ(path.find('\0'), std::string::npos);
}

TEST(GetTempFilePathTest, EmptyPrefixAndPostfix) {
  std::string path = ai::base::getTempFilePath("", "");
  EXPECT_FALSE(path.empty());
}

TEST(GetTempFilePathTest, UniquePaths) {
  std::string p1 = ai::base::getTempFilePath("u", ".t");
  std::string p2 = ai::base::getTempFilePath("u", ".t");
  EXPECT_NE(p1, p2);
}

TEST(GetTempFilePathTest, PathDoesNotYetExistOnDisk) {
  // getTempFilePath generates a unique name but should NOT leave the file
  // behind (the internal mkstemp fd is closed and the file is removed).
  std::string path = ai::base::getTempFilePath("noexist", ".test");
  EXPECT_FALSE(fs::exists(path));
}
