#include "base/file.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
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
    std::ofstream out(path_, std::ios::binary);
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
// read_file
// =============================================================================

TEST(UtilsReadFileTest, ReadsEntireFile) {
  UtilsTempTestFile f("hello\nworld\n");
  auto content = ai::base::read_file(f.path());
  ASSERT_TRUE(content.has_value());
  EXPECT_EQ(*content, "hello\nworld\n");
}

TEST(UtilsReadFileTest, ReadsEmptyFile) {
  UtilsTempTestFile f("");
  auto content = ai::base::read_file(f.path());
  ASSERT_TRUE(content.has_value());
  EXPECT_EQ(*content, "");
}

TEST(UtilsReadFileTest, ReadsSingleLine) {
  UtilsTempTestFile f("single line without newline");
  auto content = ai::base::read_file(f.path());
  ASSERT_TRUE(content.has_value());
  EXPECT_EQ(*content, "single line without newline");
}

TEST(UtilsReadFileTest, ReadsBinaryContent) {
  // Create a file with embedded null bytes (use explicit char array to
  // avoid UB from reading past a string literal's null terminator).
  const char raw[] = {'A', 'B', '\0', 'C', 'D', '\0', 'E', 'F'};
  std::string binary_data(raw, sizeof(raw));
  UtilsTempTestFile f(binary_data, ".bin");
  auto content = ai::base::read_file(f.path());
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
      ai::base::read_file("/nonexistent/path/definitely_not_a_file.txt");
  EXPECT_FALSE(content.has_value());
}

TEST(UtilsReadFileTest, DirectoryPathReturnsNullopt) {
  // read_file now explicitly rejects directories and returns nullopt
  // on all platforms (Linux/GCC would throw an exception when reading
  // from a directory stream; macOS/Clang would return garbage data).
  UtilsTempTestDir dir;
  auto content = ai::base::read_file(dir.path());
  EXPECT_FALSE(content.has_value());
}

TEST(UtilsReadFileTest, LargeFile) {
  UtilsTempTestFile f(std::string(10000, 'X'));
  auto content = ai::base::read_file(f.path());
  ASSERT_TRUE(content.has_value());
  EXPECT_EQ(content->size(), 10000u);
  EXPECT_EQ(*content, std::string(10000, 'X'));
}

// =============================================================================
// write_file
// =============================================================================

TEST(WriteFileTest, WritesContentToFile) {
  UtilsTempTestFile f("");
  EXPECT_TRUE(ai::base::write_file(f.path(), "new content"));

  auto content = ai::base::read_file(f.path());
  ASSERT_TRUE(content.has_value());
  EXPECT_EQ(*content, "new content");
}

TEST(WriteFileTest, CreatesFileIfNotExists) {
  UtilsTempTestDir dir;
  std::string path = (fs::path(dir.path()) / "new_file.txt").string();

  EXPECT_TRUE(ai::base::write_file(path, "created!"));
  EXPECT_TRUE(fs::exists(path));

  auto content = ai::base::read_file(path);
  ASSERT_TRUE(content.has_value());
  EXPECT_EQ(*content, "created!");
}

TEST(WriteFileTest, OverwritesExistingFile) {
  UtilsTempTestFile f("original");
  EXPECT_TRUE(ai::base::write_file(f.path(), "overwritten"));

  auto content = ai::base::read_file(f.path());
  ASSERT_TRUE(content.has_value());
  EXPECT_EQ(*content, "overwritten");
}

TEST(WriteFileTest, WritesEmptyContent) {
  UtilsTempTestFile f("original");
  EXPECT_TRUE(ai::base::write_file(f.path(), ""));

  auto content = ai::base::read_file(f.path());
  ASSERT_TRUE(content.has_value());
  EXPECT_EQ(*content, "");
}

TEST(WriteFileTest, WriteToNonWritableDirectoryReturnsFalse) {
  // Attempt to write to a path that has a directory component that does not
  // exist
  std::string path = "/nonexistent_dir_xyz/subdir/file.txt";
  EXPECT_FALSE(ai::base::write_file(path, "data"));
}

TEST(WriteFileTest, WriteBinaryContent) {
  UtilsTempTestFile f("");
  std::string binary_data("AB\0CD\0EF", 7);
  EXPECT_TRUE(ai::base::write_file(f.path(), binary_data));

  auto content = ai::base::read_file(f.path());
  ASSERT_TRUE(content.has_value());
  EXPECT_EQ(content->size(), 7u);
  EXPECT_EQ(*content, binary_data);
}

TEST(WriteFileTest, WriteLargeContent) {
  UtilsTempTestFile f("");
  std::string large(100000, 'Z');
  EXPECT_TRUE(ai::base::write_file(f.path(), large));

  auto content = ai::base::read_file(f.path());
  ASSERT_TRUE(content.has_value());
  EXPECT_EQ(content->size(), 100000u);
}

// =============================================================================
// read_file + write_file round-trip
// =============================================================================

TEST(ReadWriteRoundTripTest, RoundTrip) {
  UtilsTempTestFile f("");
  std::string original = "Hello, round-trip world!\nLine 2\nLine 3\n";

  EXPECT_TRUE(ai::base::write_file(f.path(), original));
  auto result = ai::base::read_file(f.path());

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, original);
}
