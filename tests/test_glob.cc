#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "ai/glob.h"

namespace fs = std::filesystem;

// =============================================================================
// Helpers
// =============================================================================

/// RAII temporary directory that cleans up on destruction.
class GlobTempDir {
 public:
  GlobTempDir()
      : path_(fs::temp_directory_path() /
              ("ai_cli_glob_test_" + std::to_string(counter_++))) {
    fs::create_directories(path_);
  }

  ~GlobTempDir() {
    std::error_code ec;
    fs::remove_all(path_, ec);
  }

  fs::path path() const { return path_; }

  /// Create a file with optional content
  void create_file(const std::string& relative_path,
                   const std::string& content = "") {
    fs::path full = path_ / relative_path;
    fs::create_directories(full.parent_path());
    std::ofstream out(full);
    out << content;
  }

  /// Create a subdirectory
  void create_dir(const std::string& relative_path) {
    fs::create_directories(path_ / relative_path);
  }

 private:
  fs::path path_;
  static inline int counter_ = 0;
};

/// Helper: get filenames from paths, sorted
std::vector<std::string> sorted_filenames(const std::vector<fs::path>& paths) {
  std::vector<std::string> names;
  for (const auto& p : paths) {
    names.push_back(p.filename().string());
  }
  std::sort(names.begin(), names.end());
  return names;
}

/// Helper: get relative paths from a base, sorted
std::vector<std::string> sorted_relative_paths(
    const std::vector<fs::path>& paths, const fs::path& base) {
  std::vector<std::string> rel;
  for (const auto& p : paths) {
    rel.push_back(fs::relative(p, base).string());
  }
  std::sort(rel.begin(), rel.end());
  return rel;
}

// =============================================================================
// Non-recursive glob tests
// =============================================================================

TEST(GlobTest, ExactFilename) {
  GlobTempDir tmp;
  tmp.create_file("hello.txt");
  tmp.create_file("world.txt");
  tmp.create_file("other.log");

  auto result = ai::glob("hello.txt", tmp.path());
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result[0].filename(), "hello.txt");
}

TEST(GlobTest, StarWildcard) {
  GlobTempDir tmp;
  tmp.create_file("a.txt");
  tmp.create_file("b.txt");
  tmp.create_file("c.log");

  auto result = ai::glob("*.txt", tmp.path());
  auto names = sorted_filenames(result);
  ASSERT_EQ(names.size(), 2u);
  EXPECT_EQ(names[0], "a.txt");
  EXPECT_EQ(names[1], "b.txt");
}

TEST(GlobTest, QuestionMarkWildcard) {
  GlobTempDir tmp;
  tmp.create_file("a.txt");
  tmp.create_file("b.txt");
  tmp.create_file("ab.txt");

  auto result = ai::glob("?.txt", tmp.path());
  auto names = sorted_filenames(result);
  ASSERT_EQ(names.size(), 2u);
  EXPECT_EQ(names[0], "a.txt");
  EXPECT_EQ(names[1], "b.txt");
}

TEST(GlobTest, MixedWildcards) {
  GlobTempDir tmp;
  tmp.create_file("test_001.txt");
  tmp.create_file("test_002.txt");
  tmp.create_file("test_abc.txt");
  tmp.create_file("prod_001.txt");

  auto result = ai::glob("test_???.txt", tmp.path());
  auto names = sorted_filenames(result);
  // test_001.txt, test_002.txt, test_abc.txt all have 3 chars between _ and .
  ASSERT_EQ(names.size(), 3u);
  EXPECT_EQ(names[0], "test_001.txt");
  EXPECT_EQ(names[1], "test_002.txt");
  EXPECT_EQ(names[2], "test_abc.txt");
}

TEST(GlobTest, NoMatch) {
  GlobTempDir tmp;
  tmp.create_file("hello.txt");

  auto result = ai::glob("*.log", tmp.path());
  EXPECT_TRUE(result.empty());
}

TEST(GlobTest, EmptyDirectory) {
  GlobTempDir tmp;

  auto result = ai::glob("*", tmp.path());
  EXPECT_TRUE(result.empty());
}

TEST(GlobTest, NonExistentDirectory) {
  fs::path nonexistent =
      fs::temp_directory_path() / "ai_cli_glob_nonexistent_dir_12345";

  auto result = ai::glob("*", nonexistent);
  EXPECT_TRUE(result.empty());
}

TEST(GlobTest, StarMatchesEverything) {
  GlobTempDir tmp;
  tmp.create_file("a.txt");
  tmp.create_file("b.log");
  tmp.create_file("c");
  tmp.create_dir("subdir");

  auto result = ai::glob("*", tmp.path());
  ASSERT_EQ(result.size(), 4u);
  auto names = sorted_filenames(result);
  EXPECT_EQ(names[0], "a.txt");
  EXPECT_EQ(names[1], "b.log");
  EXPECT_EQ(names[2], "c");
  EXPECT_EQ(names[3], "subdir");
}

TEST(GlobTest, PrefixStarPattern) {
  GlobTempDir tmp;
  tmp.create_file("my_file.txt");
  tmp.create_file("my_file.log");
  tmp.create_file("other_file.txt");

  auto result = ai::glob("my_file.*", tmp.path());
  auto names = sorted_filenames(result);
  ASSERT_EQ(names.size(), 2u);
  EXPECT_EQ(names[0], "my_file.log");
  EXPECT_EQ(names[1], "my_file.txt");
}

// =============================================================================
// Case-insensitive tests
// =============================================================================

TEST(GlobTest, CaseInsensitiveExact) {
  GlobTempDir tmp;
  // Use distinct filenames (not just case-different) because some
  // filesystems (e.g. macOS APFS) are case-insensitive.
  tmp.create_file("Alpha.txt");
  tmp.create_file("Bravo.txt");
  tmp.create_file("Charlie.txt");

  // Case-insensitive: "alpha.txt" should match "Alpha.txt"
  auto result = ai::glob("alpha.txt", tmp.path(), false, true);
  auto names = sorted_filenames(result);
  ASSERT_EQ(names.size(), 1u);
  EXPECT_EQ(names[0], "Alpha.txt");

  // "ALPHA.TXT" should also match "Alpha.txt" (case-insensitive)
  result = ai::glob("ALPHA.TXT", tmp.path(), false, true);
  names = sorted_filenames(result);
  ASSERT_EQ(names.size(), 1u);
  EXPECT_EQ(names[0], "Alpha.txt");
}

TEST(GlobTest, CaseSensitiveExact) {
  GlobTempDir tmp;
  tmp.create_file("Alpha.txt");
  tmp.create_file("bravo.txt");
  tmp.create_file("CHARLIE.TXT");

  // Case-sensitive: "Alpha.txt" matches only "Alpha.txt"
  auto result = ai::glob("Alpha.txt", tmp.path(), false, false);
  auto names = sorted_filenames(result);
  ASSERT_EQ(names.size(), 1u);
  EXPECT_EQ(names[0], "Alpha.txt");

  // "alpha.txt" does not match "Alpha.txt" (case-sensitive)
  result = ai::glob("alpha.txt", tmp.path(), false, false);
  EXPECT_TRUE(result.empty());
}

TEST(GlobTest, CaseInsensitiveWithWildcard) {
  GlobTempDir tmp;
  tmp.create_file("Report.TXT");
  tmp.create_file("Report.csv");
  tmp.create_file("Notes.log");

  auto result = ai::glob("report.*", tmp.path(), false, true);
  auto names = sorted_filenames(result);
  ASSERT_EQ(names.size(), 2u);
  EXPECT_EQ(names[0], "Report.TXT");
  EXPECT_EQ(names[1], "Report.csv");
}

// =============================================================================
// Recursive glob tests
// =============================================================================

TEST(GlobTest, RecursiveStarTxt) {
  GlobTempDir tmp;
  tmp.create_file("a.txt");
  tmp.create_file("sub/b.txt");
  tmp.create_file("sub/deep/c.txt");
  tmp.create_file("sub/d.log");
  tmp.create_file("other/e.txt");

  auto result = ai::glob("*.txt", tmp.path(), true);
  auto names = sorted_filenames(result);
  // a.txt, b.txt, c.txt, e.txt
  ASSERT_EQ(names.size(), 4u);
  // All matched files should end with .txt
  for (const auto& n : names) {
    EXPECT_TRUE(n.size() >= 4 && n.substr(n.size() - 4) == ".txt");
  }
}

TEST(GlobTest, RecursiveExactName) {
  GlobTempDir tmp;
  tmp.create_file("readme.md");
  tmp.create_file("sub/readme.md");
  tmp.create_file("sub/deep/readme.md");
  tmp.create_file("other/notes.md");

  auto result = ai::glob("readme.md", tmp.path(), true);
  ASSERT_EQ(result.size(), 3u);
}

TEST(GlobTest, RecursiveNoMatch) {
  GlobTempDir tmp;
  tmp.create_file("a.txt");
  tmp.create_file("sub/b.txt");

  auto result = ai::glob("*.log", tmp.path(), true);
  EXPECT_TRUE(result.empty());
}

TEST(GlobTest, RecursiveEmptyDir) {
  GlobTempDir tmp;

  auto result = ai::glob("*", tmp.path(), true);
  EXPECT_TRUE(result.empty());
}

TEST(GlobTest, NonRecursiveDoesNotEnterSubdirs) {
  GlobTempDir tmp;
  tmp.create_file("top.txt");
  tmp.create_file("sub/nested.txt");

  auto result = ai::glob("*.txt", tmp.path(), false);
  auto names = sorted_filenames(result);
  ASSERT_EQ(names.size(), 1u);
  EXPECT_EQ(names[0], "top.txt");
}

// =============================================================================
// Advanced pattern tests
// =============================================================================

TEST(GlobTest, DotFiles) {
  GlobTempDir tmp;
  tmp.create_file(".gitignore");
  tmp.create_file(".env");
  tmp.create_file("normal.txt");

  auto result = ai::glob(".*", tmp.path());
  auto names = sorted_filenames(result);
  ASSERT_EQ(names.size(), 2u);
  EXPECT_EQ(names[0], ".env");
  EXPECT_EQ(names[1], ".gitignore");
}

TEST(GlobTest, DotFilesRecursive) {
  GlobTempDir tmp;
  tmp.create_file(".hidden");
  tmp.create_file("sub/.hidden");

  auto result = ai::glob(".*", tmp.path(), true);
  ASSERT_EQ(result.size(), 2u);
}

TEST(GlobTest, ComplexPattern) {
  GlobTempDir tmp;
  tmp.create_file("img_001.jpg");
  tmp.create_file("img_002.jpg");
  tmp.create_file("img_abc.jpg");
  tmp.create_file("img_001.png");
  tmp.create_file("vid_001.jpg");

  auto result = ai::glob("img_???.jpg", tmp.path());
  auto names = sorted_filenames(result);
  // img_001.jpg, img_002.jpg, img_abc.jpg all have exactly 3 chars
  ASSERT_EQ(names.size(), 3u);
  EXPECT_EQ(names[0], "img_001.jpg");
  EXPECT_EQ(names[1], "img_002.jpg");
  EXPECT_EQ(names[2], "img_abc.jpg");
}

TEST(GlobTest, PatternMatchingDirectories) {
  GlobTempDir tmp;
  tmp.create_file("file.txt");
  tmp.create_dir("adir");
  tmp.create_dir("bdir");
  tmp.create_dir("cdir_extra");

  auto result = ai::glob("?dir", tmp.path());
  auto names = sorted_filenames(result);
  ASSERT_EQ(names.size(), 2u);
  EXPECT_EQ(names[0], "adir");
  EXPECT_EQ(names[1], "bdir");
}

// =============================================================================
// Error handling tests
// =============================================================================

TEST(GlobTest, PermissionErrorGraceful) {
  // Just test that glob handles the basic error path gracefully
  // by not throwing on a nonexistent subdirectory
  GlobTempDir tmp;
  tmp.create_file("exists.txt");

  // Using a path that definitely doesn't exist should return empty
  auto result = ai::glob("*", tmp.path() / "nonexistent_subdir");
  EXPECT_TRUE(result.empty());
}

TEST(GlobTest, GlobWithRelativeDir) {
  GlobTempDir tmp;
  tmp.create_file("test.txt");

  auto original_cwd = fs::current_path();
  fs::current_path(tmp.path().parent_path());
  auto result = ai::glob("*.txt", tmp.path().filename());
  fs::current_path(original_cwd);

  auto names = sorted_filenames(result);
  ASSERT_EQ(names.size(), 1u);
  EXPECT_EQ(names[0], "test.txt");
}
