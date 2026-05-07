#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <subprocess/subprocess.hpp>

#include "ai/tool_calls.h"

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
    std::ofstream out(path_);
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
// read_file tests
// =============================================================================

TEST(ReadFileTest, ReadsEntireFile) {
  TempTestFile f("hello\nworld\n");
  json args = {{"path", f.path()}};
  std::string result = call_tool("read_file", args);
  EXPECT_EQ(result, "hello\nworld\n");
}

TEST(ReadFileTest, ReadsEntireFileUsingFileParam) {
  TempTestFile f("content here");
  json args = {{"file", f.path()}};
  std::string result = call_tool("read_file", args);
  EXPECT_EQ(result, "content here");
}

TEST(ReadFileTest, FileNotExists) {
  json args = {{"path", "/nonexistent/path/file.txt"}};
  std::string result = call_tool("read_file", args);
  EXPECT_TRUE(result.find("is not exists") != std::string::npos);
}

TEST(ReadFileTest, EmptyFile) {
  TempTestFile f("");
  json args = {{"path", f.path()}};
  std::string result = call_tool("read_file", args);
  EXPECT_TRUE(result.find("is empty") != std::string::npos);
}

TEST(ReadFileTest, MissingPathAndFile) {
  json args = json::object();
  std::string result = call_tool("read_file", args);
  EXPECT_TRUE(result.find("missing required parameter") != std::string::npos);
}

TEST(ReadFileTest, NotAnObject) {
  json args = json::array();
  std::string result = call_tool("read_file", args);
  EXPECT_TRUE(result.find("expected a JSON object") != std::string::npos);
}

TEST(ReadFileTest, PathNotString) {
  json args = {{"path", 123}};
  std::string result = call_tool("read_file", args);
  EXPECT_TRUE(result.find("\"path\" must be a string") != std::string::npos);
}

TEST(ReadFileTest, FileNotString) {
  json args = {{"file", true}};
  std::string result = call_tool("read_file", args);
  EXPECT_TRUE(result.find("\"file\" must be a string") != std::string::npos);
}

TEST(ReadFileTest, OffsetAndLimit) {
  TempTestFile f("line1\nline2\nline3\nline4\nline5\n");
  json args = {{"path", f.path()}, {"offset", 2}, {"limit", 2}};
  std::string result = call_tool("read_file", args);
  EXPECT_EQ(result, "line2\nline3\n");
}

TEST(ReadFileTest, OffsetOnly) {
  TempTestFile f("line1\nline2\nline3\n");
  json args = {{"path", f.path()}, {"offset", 2}};
  std::string result = call_tool("read_file", args);
  EXPECT_EQ(result, "line2\nline3\n");
}

TEST(ReadFileTest, OffsetOutOfRange) {
  TempTestFile f("line1\nline2\n");
  json args = {{"path", f.path()}, {"offset", 10}};
  std::string result = call_tool("read_file", args);
  EXPECT_TRUE(result.find("is out of range") != std::string::npos);
}

TEST(ReadFileTest, LimitZero) {
  TempTestFile f("line1\nline2\n");
  json args = {{"path", f.path()}, {"limit", 0}};
  std::string result = call_tool("read_file", args);
  EXPECT_EQ(result, "");
}

TEST(ReadFileTest, LimitExceedsFile) {
  TempTestFile f("line1\nline2\n");
  json args = {{"path", f.path()}, {"limit", 100}};
  std::string result = call_tool("read_file", args);
  EXPECT_EQ(result, "line1\nline2\n");
}

TEST(ReadFileTest, LimitWithoutOffset) {
  TempTestFile f("line1\nline2\nline3\n");
  json args = {{"path", f.path()}, {"limit", 1}};
  std::string result = call_tool("read_file", args);
  EXPECT_EQ(result, "line1\n");
}

TEST(ReadFileTest, OffsetClampedToAtLeast1) {
  TempTestFile f("line1\nline2\n");
  json args = {{"path", f.path()}, {"offset", 0}, {"limit", 1}};
  std::string result = call_tool("read_file", args);
  EXPECT_EQ(result, "line1\n");
}

// =============================================================================
// read_multiple_files tests
// =============================================================================

TEST(ReadMultipleFilesTest, ReadsMultipleFiles) {
  TempTestFile f1("content1");
  TempTestFile f2("content2");
  json args = {{"paths", json::array({f1.path(), f2.path()})}};
  std::string result = call_tool("read_multiple_files", args);
  EXPECT_TRUE(result.find("content1") != std::string::npos);
  EXPECT_TRUE(result.find("content2") != std::string::npos);
  EXPECT_TRUE(result.find("------") != std::string::npos);
}

TEST(ReadMultipleFilesTest, FileNotFound) {
  json args = {{"paths", json::array({"/nonexistent/file.txt"})}};
  std::string result = call_tool("read_multiple_files", args);
  EXPECT_TRUE(result.find("failed to read") != std::string::npos);
}

TEST(ReadMultipleFilesTest, EmptyPaths) {
  json args = {{"paths", json::array()}};
  std::string result = call_tool("read_multiple_files", args);
  EXPECT_EQ(result, "");
}

TEST(ReadMultipleFilesTest, NotAnObject) {
  json args = json::array();
  std::string result = call_tool("read_multiple_files", args);
  EXPECT_TRUE(result.find("expected a JSON object") != std::string::npos);
}

TEST(ReadMultipleFilesTest, MissingPaths) {
  json args = json::object();
  std::string result = call_tool("read_multiple_files", args);
  EXPECT_TRUE(result.find("missing required parameter") != std::string::npos);
}

TEST(ReadMultipleFilesTest, PathsNotArray) {
  json args = {{"paths", "not_an_array"}};
  std::string result = call_tool("read_multiple_files", args);
  EXPECT_TRUE(result.find("must be an array") != std::string::npos);
}

// =============================================================================
// write_file tests
// =============================================================================

TEST(WriteFileTest, WritesFileSuccessfully) {
  TempTestFile f("");
  json args = {{"path", f.path()}, {"content", "new content"}};
  std::string result = call_tool("write_file", args);
  EXPECT_TRUE(result.find("Successfully wrote") != std::string::npos);

  std::ifstream in(f.path());
  std::string content{std::istreambuf_iterator<char>(in),
                      std::istreambuf_iterator<char>()};
  EXPECT_EQ(content, "new content");
}

TEST(WriteFileTest, NotAnObject) {
  json args = json::array();
  std::string result = call_tool("write_file", args);
  EXPECT_TRUE(result.find("expected a JSON object") != std::string::npos);
}

TEST(WriteFileTest, MissingPath) {
  json args = {{"content", "hello"}};
  std::string result = call_tool("write_file", args);
  EXPECT_TRUE(result.find("missing required parameter") != std::string::npos);
}

TEST(WriteFileTest, MissingContent) {
  TempTestFile f("");
  json args = {{"path", f.path()}};
  std::string result = call_tool("write_file", args);
  EXPECT_TRUE(result.find("missing required parameter") != std::string::npos);
}

TEST(WriteFileTest, PathNotString) {
  json args = {{"path", 42}, {"content", "hello"}};
  std::string result = call_tool("write_file", args);
  EXPECT_TRUE(result.find("\"path\" must be a string") != std::string::npos);
}

TEST(WriteFileTest, ContentNotString) {
  TempTestFile f("");
  json args = {{"path", f.path()}, {"content", 123}};
  std::string result = call_tool("write_file", args);
  EXPECT_TRUE(result.find("\"content\" must be a string") != std::string::npos);
}

// =============================================================================
// edit_file tests
// =============================================================================

TEST(EditFileTest, SingleSearchReplace) {
  TempTestFile f("hello world\n");
  std::string diff =
      "<<<<<<< SEARCH\nhello world\n=======\ngoodbye world\n>>>>>>> REPLACE\n";
  json args = {{"path", f.path()}, {"diff", diff}};
  std::string result = call_tool("edit_file", args);
  EXPECT_TRUE(result.find("Successfully edited") != std::string::npos);

  std::ifstream in(f.path());
  std::string content{std::istreambuf_iterator<char>(in),
                      std::istreambuf_iterator<char>()};
  EXPECT_EQ(content, "goodbye world\n");
}

TEST(EditFileTest, MultipleSearchReplaceBlocks) {
  TempTestFile f("line A\nline B\nline C\n");
  std::string diff =
      "<<<<<<< SEARCH\nline A\n=======\nreplaced A\n>>>>>>> REPLACE\n"
      "<<<<<<< SEARCH\nline C\n=======\nreplaced C\n>>>>>>> REPLACE\n";
  json args = {{"path", f.path()}, {"diff", diff}};
  std::string result = call_tool("edit_file", args);
  EXPECT_TRUE(result.find("Successfully edited") != std::string::npos);

  std::ifstream in(f.path());
  std::string content{std::istreambuf_iterator<char>(in),
                      std::istreambuf_iterator<char>()};
  EXPECT_EQ(content, "replaced A\nline B\nreplaced C\n");
}

TEST(EditFileTest, SearchNotFound) {
  TempTestFile f("original content\n");
  std::string diff =
      "<<<<<<< SEARCH\nnonexistent text\n=======\nreplacement\n>>>>>>> "
      "REPLACE\n";
  json args = {{"path", f.path()}, {"diff", diff}};
  std::string result = call_tool("edit_file", args);
  EXPECT_TRUE(result.find("Failed to edit file") != std::string::npos);
}

TEST(EditFileTest, FileNotExists) {
  std::string diff = "<<<<<<< SEARCH\ntext\n=======\nnew\n>>>>>>> REPLACE\n";
  json args = {{"path", "/nonexistent/edit_file_test.txt"}, {"diff", diff}};
  std::string result = call_tool("edit_file", args);
  EXPECT_TRUE(result.find("Failed to open file") != std::string::npos);
}

TEST(EditFileTest, NotAnObject) {
  json args = json::array();
  std::string result = call_tool("edit_file", args);
  EXPECT_TRUE(result.find("expected a JSON object") != std::string::npos);
}

TEST(EditFileTest, MissingPath) {
  json args = {{"diff", "some diff"}};
  std::string result = call_tool("edit_file", args);
  EXPECT_TRUE(result.find("missing required parameter") != std::string::npos);
}

TEST(EditFileTest, MissingDiff) {
  TempTestFile f("content");
  json args = {{"path", f.path()}};
  std::string result = call_tool("edit_file", args);
  EXPECT_TRUE(result.find("missing required parameter") != std::string::npos);
}

TEST(EditFileTest, PathNotString) {
  json args = {{"path", 1}, {"diff", "diff"}};
  std::string result = call_tool("edit_file", args);
  EXPECT_TRUE(result.find("\"path\" must be a string") != std::string::npos);
}

TEST(EditFileTest, DiffNotString) {
  TempTestFile f("content");
  json args = {{"path", f.path()}, {"diff", false}};
  std::string result = call_tool("edit_file", args);
  EXPECT_TRUE(result.find("\"diff\" must be a string") != std::string::npos);
}

TEST(EditFileTest, MissingSplitLabel) {
  TempTestFile f("content");
  std::string diff = "<<<<<<< SEARCH\ncontent\n>>>>>>> REPLACE\n";
  json args = {{"path", f.path()}, {"diff", diff}};
  std::string result = call_tool("edit_file", args);
  EXPECT_TRUE(result.find("Failed to edit file") != std::string::npos);
}

TEST(EditFileTest, DiffNotStartWithSearchLabel) {
  TempTestFile f("content");
  // diff is just random text, does not start with "<<<<<<< SEARCH\n"
  std::string diff = "not a valid diff";
  json args = {{"path", f.path()}, {"diff", diff}};
  std::string result = call_tool("edit_file", args);
  EXPECT_TRUE(result.find("diff must start with") != std::string::npos);
}

TEST(EditFileTest, DiffEmpty) {
  TempTestFile f("content");
  std::string diff = "";
  json args = {{"path", f.path()}, {"diff", diff}};
  std::string result = call_tool("edit_file", args);
  EXPECT_TRUE(result.find("diff must start with") != std::string::npos);
}

TEST(EditFileTest, DiffSearchLabelNotFollowedByNewline) {
  TempTestFile f("content");
  // starts with "<<<<<<< SEARCH" but not followed by newline
  std::string diff = "<<<<<<< SEARCH old content";
  json args = {{"path", f.path()}, {"diff", diff}};
  std::string result = call_tool("edit_file", args);
  EXPECT_TRUE(result.find("diff must start with") != std::string::npos);
}

TEST(EditFileTest, MismatchedLabelCounts) {
  TempTestFile f("line A\nline B\n");
  // 2 SEARCH blocks but only 1 SEPARATOR and 1 REPLACE
  std::string diff =
      "<<<<<<< SEARCH\nline A\n=======\nreplaced A\n>>>>>>> REPLACE\n"
      "<<<<<<< SEARCH\nline B\n";
  json args = {{"path", f.path()}, {"diff", diff}};
  std::string result = call_tool("edit_file", args);
  EXPECT_TRUE(result.find("mismatched number of SEARCH/SEPARATOR/REPLACE "
                          "labels") != std::string::npos);
}

TEST(EditFileTest, LabelsOutOfOrder) {
  TempTestFile f("foo\nbar\n");
  // REPLACE label appears before SEPARATOR label in the block
  std::string diff = "<<<<<<< SEARCH\nfoo\n>>>>>>> REPLACE\n=======\nbar\n";
  json args = {{"path", f.path()}, {"diff", diff}};
  std::string result = call_tool("edit_file", args);
  EXPECT_TRUE(result.find("labels are out of order") != std::string::npos);
}

TEST(EditFileTest, EmptyReplaceBlock) {
  TempTestFile f("hello world\nfoo bar\nbaz qux\n");
  // adjacent "=======" and ">>>>>>> REPLACE" means delete the SEARCH content
  std::string diff = "<<<<<<< SEARCH\nfoo bar\n=======\n>>>>>>> REPLACE\n";
  json args = {{"path", f.path()}, {"diff", diff}};
  std::string result = call_tool("edit_file", args);
  EXPECT_TRUE(result.find("Successfully edited") != std::string::npos);

  std::ifstream in(f.path());
  std::string content{std::istreambuf_iterator<char>(in),
                      std::istreambuf_iterator<char>()};
  // "foo bar\n" should have been deleted
  EXPECT_EQ(content, "hello world\nbaz qux\n");
}

TEST(EditFileTest, MixedEmptyAndNonEmptyBlocks) {
  TempTestFile f("keep this\nremove me\nreplace this\nlast line\n");
  // Block 1: delete "remove me\n"
  // Block 2: replace "replace this\n" with "new content\n"
  std::string diff =
      "<<<<<<< SEARCH\nremove me\n=======\n>>>>>>> REPLACE\n"
      "<<<<<<< SEARCH\nreplace this\n=======\nnew content\n>>>>>>> REPLACE\n";
  json args = {{"path", f.path()}, {"diff", diff}};
  std::string result = call_tool("edit_file", args);
  EXPECT_TRUE(result.find("Successfully edited") != std::string::npos);

  std::ifstream in(f.path());
  std::string content{std::istreambuf_iterator<char>(in),
                      std::istreambuf_iterator<char>()};
  EXPECT_EQ(content, "keep this\nnew content\nlast line\n");
}

TEST(EditFileTest, MismatchedLabelCountsExtraReplace) {
  TempTestFile f("content A\ncontent B\n");
  // Extra REPLACE label without corresponding SEARCH/SEPARATOR
  std::string diff =
      "<<<<<<< SEARCH\ncontent A\n=======\nnew A\n>>>>>>> REPLACE\n"
      ">>>>>>> REPLACE\n";
  json args = {{"path", f.path()}, {"diff", diff}};
  std::string result = call_tool("edit_file", args);
  EXPECT_TRUE(result.find("mismatched number of SEARCH/SEPARATOR/REPLACE "
                          "labels") != std::string::npos);
}

// =============================================================================
// create_directory tests
// =============================================================================

TEST(CreateDirectoryTest, CreatesDirectory) {
  TempTestDir parent;
  std::string new_dir =
      (std::filesystem::path(parent.path()) / "subdir").string();
  json args = {{"path", new_dir}};
  std::string result = call_tool("create_directory", args);
  EXPECT_TRUE(result.find("Successfully created directory") !=
              std::string::npos);
  EXPECT_TRUE(std::filesystem::exists(new_dir));
  EXPECT_TRUE(std::filesystem::is_directory(new_dir));
}

TEST(CreateDirectoryTest, CreatesNestedDirectories) {
  TempTestDir parent;
  std::string nested =
      (std::filesystem::path(parent.path()) / "a" / "b" / "c").string();
  json args = {{"path", nested}};
  std::string result = call_tool("create_directory", args);
  EXPECT_TRUE(result.find("Successfully created directory") !=
              std::string::npos);
  EXPECT_TRUE(std::filesystem::exists(nested));
}

TEST(CreateDirectoryTest, AlreadyExists) {
  TempTestDir dir;
  json args = {{"path", dir.path()}};
  std::string result = call_tool("create_directory", args);
  EXPECT_TRUE(result.find("Successfully created directory") !=
              std::string::npos);
}

TEST(CreateDirectoryTest, NotAnObject) {
  json args = json::array();
  std::string result = call_tool("create_directory", args);
  EXPECT_TRUE(result.find("expected a JSON object") != std::string::npos);
}

TEST(CreateDirectoryTest, MissingPath) {
  json args = json::object();
  std::string result = call_tool("create_directory", args);
  EXPECT_TRUE(result.find("missing required parameter") != std::string::npos);
}

TEST(CreateDirectoryTest, PathNotString) {
  json args = {{"path", 12345}};
  std::string result = call_tool("create_directory", args);
  EXPECT_TRUE(result.find("\"path\" must be a string") != std::string::npos);
}

// =============================================================================
// list_directory tests
// =============================================================================

TEST(ListDirectoryTest, ListsFilesAndDirs) {
  TempTestDir dir;
  std::ofstream(
      (std::filesystem::path(dir.path()) / "test_file.txt").string())
      << "data";
  std::filesystem::create_directory(
      std::filesystem::path(dir.path()) / "test_subdir");

  json args = {{"path", dir.path()}};
  std::string result = call_tool("list_directory", args);
  EXPECT_TRUE(result.find("[FILE]") != std::string::npos);
  EXPECT_TRUE(result.find("[DIR]") != std::string::npos);
  EXPECT_TRUE(result.find("test_file.txt") != std::string::npos);
  EXPECT_TRUE(result.find("test_subdir") != std::string::npos);
}

TEST(ListDirectoryTest, NotADirectory) {
  TempTestFile f("content");
  json args = {{"path", f.path()}};
  std::string result = call_tool("list_directory", args);
  EXPECT_TRUE(result.find("Error") != std::string::npos);
}

TEST(ListDirectoryTest, NotExists) {
  json args = {{"path", "/nonexistent/directory"}};
  std::string result = call_tool("list_directory", args);
  EXPECT_TRUE(result.find("Error") != std::string::npos);
}

TEST(ListDirectoryTest, NotAnObject) {
  json args = json::array();
  std::string result = call_tool("list_directory", args);
  EXPECT_TRUE(result.find("expected a JSON object") != std::string::npos);
}

TEST(ListDirectoryTest, MissingPath) {
  json args = json::object();
  std::string result = call_tool("list_directory", args);
  EXPECT_TRUE(result.find("missing required parameter") != std::string::npos);
}

TEST(ListDirectoryTest, PathNotString) {
  json args = {{"path", true}};
  std::string result = call_tool("list_directory", args);
  EXPECT_TRUE(result.find("\"path\" must be a string") != std::string::npos);
}

// =============================================================================
// directory_tree tests
// =============================================================================

TEST(DirectoryTreeTest, ReturnsJsonTree) {
  TempTestDir dir;
  std::ofstream(
      (std::filesystem::path(dir.path()) / "file.txt").string())
      << "data";
  std::filesystem::create_directory(
      std::filesystem::path(dir.path()) / "subdir");
  std::ofstream(
      (std::filesystem::path(dir.path()) / "subdir" / "nested.txt").string())
      << "nested";

  json args = {{"path", dir.path()}};
  std::string result = call_tool("directory_tree", args);

  auto tree = json::parse(result);
  EXPECT_TRUE(tree.is_array());
  EXPECT_EQ(tree.size(), 2);

  bool found_file = false;
  bool found_dir = false;
  for (auto const& node : tree) {
    if (node["name"] == "file.txt") {
      EXPECT_EQ(node["type"], "file");
      found_file = true;
    } else if (node["name"] == "subdir") {
      EXPECT_EQ(node["type"], "directory");
      EXPECT_TRUE(node.contains("children"));
      EXPECT_TRUE(node["children"].is_array());
      found_dir = true;
    }
  }
  EXPECT_TRUE(found_file);
  EXPECT_TRUE(found_dir);
}

TEST(DirectoryTreeTest, NotADirectory) {
  TempTestFile f("content");
  json args = {{"path", f.path()}};
  std::string result = call_tool("directory_tree", args);
  EXPECT_TRUE(result.find("not a directory or not exists") !=
              std::string::npos);
}

TEST(DirectoryTreeTest, NotExists) {
  json args = {{"path", "/nonexistent/dir"}};
  std::string result = call_tool("directory_tree", args);
  EXPECT_TRUE(result.find("not a directory or not exists") !=
              std::string::npos);
}

TEST(DirectoryTreeTest, NotAnObject) {
  json args = json::array();
  std::string result = call_tool("directory_tree", args);
  EXPECT_TRUE(result.find("expected a JSON object") != std::string::npos);
}

TEST(DirectoryTreeTest, MissingPath) {
  json args = json::object();
  std::string result = call_tool("directory_tree", args);
  EXPECT_TRUE(result.find("missing required parameter") != std::string::npos);
}

// =============================================================================
// move_file tests
// =============================================================================

TEST(MoveFileTest, MovesFile) {
  TempTestFile f("move me");
  TempTestDir dir;
  std::string dest =
      (std::filesystem::path(dir.path()) / "moved.txt").string();

  json args = {{"source", f.path()}, {"distination", dest}};
  std::string result = call_tool("move_file", args);
  EXPECT_TRUE(result.find("Successfully moved") != std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(f.path()));
  EXPECT_TRUE(std::filesystem::exists(dest));

  std::ifstream in(dest);
  std::string content{std::istreambuf_iterator<char>(in),
                      std::istreambuf_iterator<char>()};
  EXPECT_EQ(content, "move me");
}

TEST(MoveFileTest, SourceNotExists) {
  TempTestDir dir;
  std::string dest =
      (std::filesystem::path(dir.path()) / "dest.txt").string();
  json args = {{"source", "/nonexistent/source.txt"}, {"distination", dest}};
  std::string result = call_tool("move_file", args);
  EXPECT_TRUE(result.find("Error") != std::string::npos);
}

TEST(MoveFileTest, NotAnObject) {
  json args = json::array();
  std::string result = call_tool("move_file", args);
  EXPECT_TRUE(result.find("expected a JSON object") != std::string::npos);
}

TEST(MoveFileTest, MissingSource) {
  json args = {{"distination", "/tmp/dest.txt"}};
  std::string result = call_tool("move_file", args);
  EXPECT_TRUE(result.find("missing required parameter") != std::string::npos);
}

TEST(MoveFileTest, MissingDistination) {
  json args = {{"source", "/tmp/source.txt"}};
  std::string result = call_tool("move_file", args);
  EXPECT_TRUE(result.find("missing required parameter") != std::string::npos);
}

TEST(MoveFileTest, SourceNotString) {
  json args = {{"source", 1}, {"distination", "/tmp/dest.txt"}};
  std::string result = call_tool("move_file", args);
  EXPECT_TRUE(result.find("\"source\" must be a string") != std::string::npos);
}

TEST(MoveFileTest, DistinationNotString) {
  json args = {{"source", "/tmp/source.txt"}, {"distination", 2}};
  std::string result = call_tool("move_file", args);
  EXPECT_TRUE(result.find("\"distination\" must be a string") !=
              std::string::npos);
}

// =============================================================================
// search_files tests
// =============================================================================

TEST(SearchFilesTest, FindsFilesByPattern) {
  TempTestDir dir;
  std::ofstream(
      (std::filesystem::path(dir.path()) / "apple.txt").string())
      << "";
  std::ofstream(
      (std::filesystem::path(dir.path()) / "banana.txt").string())
      << "";
  std::ofstream(
      (std::filesystem::path(dir.path()) / "apple.csv").string())
      << "";
  std::filesystem::create_directory(
      std::filesystem::path(dir.path()) / "subdir");
  std::ofstream(
      (std::filesystem::path(dir.path()) / "subdir" / "apple_sub.txt")
          .string())
      << "";

  json args = {{"path", dir.path()}, {"pattern", "*.txt"}, {"recursive", true}};
  std::string result = call_tool("search_files", args);
  EXPECT_TRUE(result.find("apple.txt") != std::string::npos);
  EXPECT_TRUE(result.find("banana.txt") != std::string::npos);
  EXPECT_TRUE(result.find("apple.csv") == std::string::npos);
}

TEST(SearchFilesTest, NonRecursive) {
  TempTestDir dir;
  std::ofstream(
      (std::filesystem::path(dir.path()) / "root.txt").string())
      << "";
  std::filesystem::create_directory(
      std::filesystem::path(dir.path()) / "sub");
  std::ofstream(
      (std::filesystem::path(dir.path()) / "sub" / "nested.txt").string())
      << "";

  json args = {
      {"path", dir.path()}, {"pattern", "*.txt"}, {"recursive", false}};
  std::string result = call_tool("search_files", args);
  EXPECT_TRUE(result.find("root.txt") != std::string::npos);
  EXPECT_TRUE(result.find("nested.txt") == std::string::npos);
}

TEST(SearchFilesTest, NoMatch) {
  TempTestDir dir;
  json args = {{"path", dir.path()}, {"pattern", "xyz_does_not_exist_*"}};
  std::string result = call_tool("search_files", args);
  EXPECT_TRUE(result.find("No files or directories matching") !=
              std::string::npos);
}

TEST(SearchFilesTest, PartialPatternFallback) {
  TempTestDir dir;
  std::ofstream(
      (std::filesystem::path(dir.path()) / "hello_world.txt").string())
      << "";

  json args = {{"path", dir.path()}, {"pattern", "world"}};
  std::string result = call_tool("search_files", args);
  EXPECT_TRUE(result.find("hello_world.txt") != std::string::npos);
}

TEST(SearchFilesTest, NotAnObject) {
  json args = json::array();
  std::string result = call_tool("search_files", args);
  EXPECT_TRUE(result.find("expected a JSON object") != std::string::npos);
}

TEST(SearchFilesTest, MissingPath) {
  json args = {{"pattern", "*.txt"}};
  std::string result = call_tool("search_files", args);
  EXPECT_TRUE(result.find("missing required parameter") != std::string::npos);
}

TEST(SearchFilesTest, MissingPattern) {
  json args = {{"path", "/tmp"}};
  std::string result = call_tool("search_files", args);
  EXPECT_TRUE(result.find("missing required parameter") != std::string::npos);
}

// =============================================================================
// get_file_info tests
// =============================================================================

TEST(GetFileInfoTest, FileInfo) {
  TempTestFile f("hello world");
  json args = {{"path", f.path()}};
  std::string result = call_tool("get_file_info", args);
  auto info = json::parse(result);
  EXPECT_EQ(info["path"], f.path());
  EXPECT_EQ(info["type"], "file");
  EXPECT_EQ(info["size"], 11);
  EXPECT_TRUE(info.contains("last_modified"));
  EXPECT_TRUE(info.contains("permissions"));
}

TEST(GetFileInfoTest, DirectoryInfo) {
  TempTestDir dir;
  json args = {{"path", dir.path()}};
  std::string result = call_tool("get_file_info", args);
  auto info = json::parse(result);
  EXPECT_EQ(info["type"], "directory");
}

TEST(GetFileInfoTest, NotExists) {
  json args = {{"path", "/nonexistent/file.txt"}};
  std::string result = call_tool("get_file_info", args);
  EXPECT_TRUE(result.find("does not exist") != std::string::npos);
}

TEST(GetFileInfoTest, NotAnObject) {
  json args = json::array();
  std::string result = call_tool("get_file_info", args);
  EXPECT_TRUE(result.find("expected a JSON object") != std::string::npos);
}

TEST(GetFileInfoTest, MissingPath) {
  json args = json::object();
  std::string result = call_tool("get_file_info", args);
  EXPECT_TRUE(result.find("missing required parameter") != std::string::npos);
}

// =============================================================================
// disk_space_info tests
// =============================================================================

TEST(DiskSpaceInfoTest, ReturnsSpaceInfo) {
  TempTestDir dir;
  json args = {{"path", dir.path()}};
  std::string result = call_tool("disk_space_info", args);
  auto info = json::parse(result);
  EXPECT_EQ(info["path"], dir.path());
  EXPECT_TRUE(info.contains("capacity"));
  EXPECT_TRUE(info.contains("free"));
  EXPECT_TRUE(info.contains("available"));
  EXPECT_TRUE(info.contains("used"));
  EXPECT_TRUE(info.contains("capacity_human"));
  EXPECT_TRUE(info.contains("free_human"));
  EXPECT_TRUE(info.contains("available_human"));
  EXPECT_TRUE(info.contains("used_human"));
  EXPECT_TRUE(info["capacity"].get<uintmax_t>() > 0);
}

TEST(DiskSpaceInfoTest, NotAnObject) {
  json args = json::array();
  std::string result = call_tool("disk_space_info", args);
  EXPECT_TRUE(result.find("expected a JSON object") != std::string::npos);
}

TEST(DiskSpaceInfoTest, MissingPath) {
  json args = json::object();
  std::string result = call_tool("disk_space_info", args);
  EXPECT_TRUE(result.find("missing required parameter") != std::string::npos);
}

// =============================================================================
// replace_lines tests
// =============================================================================

TEST(ReplaceLinesTest, ReplacesSingleLine) {
  TempTestFile f("line1\nline2\nline3\n");
  json args = {{"path", f.path()},
               {"start_line", 2},
               {"end_line", 2},
               {"content", "REPLACED\n"}};
  std::string result = call_tool("replace_lines", args);
  EXPECT_TRUE(result.find("Successfully replaced") != std::string::npos);

  std::ifstream in(f.path());
  std::string content{std::istreambuf_iterator<char>(in),
                      std::istreambuf_iterator<char>()};
  EXPECT_EQ(content, "line1\nREPLACED\nline3\n");
}

TEST(ReplaceLinesTest, ReplacesLineRange) {
  TempTestFile f("line1\nline2\nline3\nline4\n");
  json args = {{"path", f.path()},
               {"start_line", 2},
               {"end_line", 3},
               {"content", "A\nB\n"}};
  std::string result = call_tool("replace_lines", args);
  EXPECT_TRUE(result.find("Successfully replaced") != std::string::npos);

  std::ifstream in(f.path());
  std::string content{std::istreambuf_iterator<char>(in),
                      std::istreambuf_iterator<char>()};
  EXPECT_EQ(content, "line1\nA\nB\nline4\n");
}

TEST(ReplaceLinesTest, StartLineOutOfRange) {
  TempTestFile f("line1\nline2\n");
  json args = {{"path", f.path()},
               {"start_line", 10},
               {"end_line", 10},
               {"content", "x"}};
  std::string result = call_tool("replace_lines", args);
  EXPECT_TRUE(result.find("is out of range") != std::string::npos);
}

TEST(ReplaceLinesTest, EndLineOutOfRange) {
  TempTestFile f("line1\nline2\n");
  json args = {{"path", f.path()},
               {"start_line", 1},
               {"end_line", 10},
               {"content", "x"}};
  std::string result = call_tool("replace_lines", args);
  EXPECT_TRUE(result.find("is out of range") != std::string::npos);
}

TEST(ReplaceLinesTest, StartLineLessThan1) {
  TempTestFile f("content");
  json args = {
      {"path", f.path()}, {"start_line", 0}, {"end_line", 1}, {"content", "x"}};
  std::string result = call_tool("replace_lines", args);
  EXPECT_TRUE(result.find("\"start_line\" must be >= 1") != std::string::npos);
}

TEST(ReplaceLinesTest, EndLineLessThanStartLine) {
  TempTestFile f("content");
  json args = {
      {"path", f.path()}, {"start_line", 3}, {"end_line", 1}, {"content", "x"}};
  std::string result = call_tool("replace_lines", args);
  EXPECT_TRUE(result.find("must be >= \"start_line\"") != std::string::npos);
}

TEST(ReplaceLinesTest, FileNotExists) {
  json args = {{"path", "/nonexistent/file.txt"},
               {"start_line", 1},
               {"end_line", 1},
               {"content", "x"}};
  std::string result = call_tool("replace_lines", args);
  EXPECT_TRUE(result.find("Failed to open file") != std::string::npos);
}

TEST(ReplaceLinesTest, NotAnObject) {
  json args = json::array();
  std::string result = call_tool("replace_lines", args);
  EXPECT_TRUE(result.find("expected a JSON object") != std::string::npos);
}

TEST(ReplaceLinesTest, MissingPath) {
  json args = {{"start_line", 1}, {"end_line", 1}, {"content", "x"}};
  std::string result = call_tool("replace_lines", args);
  EXPECT_TRUE(result.find("missing required parameter") != std::string::npos);
}

TEST(ReplaceLinesTest, ContentNotString) {
  TempTestFile f("content");
  json args = {
      {"path", f.path()}, {"start_line", 1}, {"end_line", 1}, {"content", 42}};
  std::string result = call_tool("replace_lines", args);
  EXPECT_TRUE(result.find("\"content\" must be a string") != std::string::npos);
}

// =============================================================================
// execute_file tests
// =============================================================================

TEST(ExecuteFileTest, ExecutesScript) {
  TempTestDir dir;
#if defined(_WIN32)
  std::string script_path =
      (std::filesystem::path(dir.path()) / "test_script.bat").string();
  {
    // Use binary mode to avoid \n → \r\n translation, so the explicit
    // \r\n line endings are preserved exactly as intended for cmd.exe.
    std::ofstream script(script_path, std::ios::binary);
    script << "@echo off\r\necho hello\r\necho error>&2\r\nexit /b 42\r\n";
  }
#else
  std::string script_path =
      (std::filesystem::path(dir.path()) / "test_script.sh").string();
  {
    std::ofstream script(script_path);
    script << "#!/bin/sh\necho hello\necho error >&2\nexit 42\n";
  }
  std::filesystem::permissions(script_path, std::filesystem::perms::owner_exec,
                               std::filesystem::perm_options::add);
#endif

  json args = {{"path", script_path}};
  std::string result = call_tool("execute_file", args);
  EXPECT_TRUE(result.find("Exit code: 42") != std::string::npos);
  EXPECT_TRUE(result.find("hello") != std::string::npos);
  EXPECT_TRUE(result.find("error") != std::string::npos);
}

TEST(ExecuteFileTest, WithArgs) {
  TempTestDir dir;
#if defined(_WIN32)
  std::string script_path =
      (std::filesystem::path(dir.path()) / "echo_args.bat").string();
  {
    // Use binary mode to avoid \n → \r\n translation, so the explicit
    // \r\n line endings are preserved exactly as intended for cmd.exe.
    std::ofstream script(script_path, std::ios::binary);
    script << "@echo off\r\necho \"%1\"\r\n";
  }
#else
  std::string script_path =
      (std::filesystem::path(dir.path()) / "echo_args.sh").string();
  {
    std::ofstream script(script_path);
    script << "#!/bin/sh\necho \"$1\"\n";
  }
  std::filesystem::permissions(script_path, std::filesystem::perms::owner_exec,
                               std::filesystem::perm_options::add);
#endif

  json args = {{"path", script_path}, {"args", json::array({"hello_arg"})}};
  std::string result = call_tool("execute_file", args);
  EXPECT_TRUE(result.find("hello_arg") != std::string::npos);
}

TEST(ExecuteFileTest, NotAnObject) {
  json args = json::array();
  std::string result = call_tool("execute_file", args);
  EXPECT_TRUE(result.find("expected a JSON object") != std::string::npos);
}

TEST(ExecuteFileTest, MissingPath) {
  json args = json::object();
  std::string result = call_tool("execute_file", args);
  EXPECT_TRUE(result.find("missing required parameter") != std::string::npos);
}
