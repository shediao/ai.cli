#include <gtest/gtest.h>

#include <nlohmann/json.hpp>
#include <string>

#include "tools/execute.h"

using json = nlohmann::json;

// =============================================================================
// filter_lines unit tests
// =============================================================================

// ── Empty / trivial input ──────────────────────────────────────────────────

TEST(FilterLinesTest, EmptyText) {
  EXPECT_EQ(ai::filter_lines("", json::array()), "");
}

TEST(FilterLinesTest, NonArrayFiltersReturnsTextUnchanged) {
  EXPECT_EQ(ai::filter_lines("hello", json::object()), "hello");
  EXPECT_EQ(ai::filter_lines("hello", json(42)), "hello");
  EXPECT_EQ(ai::filter_lines("hello", json("string")), "hello");
}

TEST(FilterLinesTest, EmptyFiltersArrayReturnsTextUnchanged) {
  EXPECT_EQ(ai::filter_lines("hello", json::array()), "hello");
}

// ── Basic split / join round-trip ──────────────────────────────────────────

TEST(FilterLinesTest, SingleLineNoTrailingNewline) {
  EXPECT_EQ(ai::filter_lines("hello", json::array()), "hello");
}

TEST(FilterLinesTest, SingleLineWithTrailingNewline) {
  // Trailing newline is dropped: "hello\n" → "hello" (getline semantics)
  EXPECT_EQ(ai::filter_lines("hello\n", json::array()), "hello");
}

TEST(FilterLinesTest, MultipleLines) {
  EXPECT_EQ(ai::filter_lines("a\nb\nc", json::array()), "a\nb\nc");
}

TEST(FilterLinesTest, MultipleLinesWithTrailingNewline) {
  EXPECT_EQ(ai::filter_lines("a\nb\nc\n", json::array()), "a\nb\nc");
}

TEST(FilterLinesTest, OnlyNewline) {
  // A single "\n" is one empty line, joined as empty string (no separator
  // needed between zero lines and one line).
  EXPECT_EQ(ai::filter_lines("\n", json::array()), "");
}

TEST(FilterLinesTest, OnlyTwoNewlines) {
  // Two empty lines joined with a single '\n' separator.
  EXPECT_EQ(ai::filter_lines("\n\n", json::array()), "\n");
}

TEST(FilterLinesTest, StartsWithNewline) {
  // "\na" → empty line + "a"
  EXPECT_EQ(ai::filter_lines("\na", json::array()), "\na");
}

TEST(FilterLinesTest, EmptyLinesInterspersed) {
  EXPECT_EQ(ai::filter_lines("a\n\nb", json::array()), "a\n\nb");
}

// ── Windows-style \r\n line endings ────────────────────────────────────────

TEST(FilterLinesTest, WindowsLineEndings) {
  // Split on '\n' only, so '\r' stays as part of the line content.
  // Each line includes its trailing '\r'. The last line "c\r" preserves
  // its '\r' because the function does not strip carriage returns.
  EXPECT_EQ(ai::filter_lines("a\r\nb\r\nc\r\n", json::array()),
            "a\r\nb\r\nc\r");
}

// ── head filter ────────────────────────────────────────────────────────────

TEST(FilterLinesTest, HeadBasic) {
  json filters = json::array({{{"head", 2}}});
  EXPECT_EQ(ai::filter_lines("line1\nline2\nline3\nline4\nline5", filters),
            "line1\nline2");
}

TEST(FilterLinesTest, HeadZero) {
  json filters = json::array({{{"head", 0}}});
  EXPECT_EQ(ai::filter_lines("line1\nline2\nline3", filters), "");
}

TEST(FilterLinesTest, HeadExactCount) {
  // head:3 on 3 lines → all kept
  json filters = json::array({{{"head", 3}}});
  EXPECT_EQ(ai::filter_lines("a\nb\nc", filters), "a\nb\nc");
}

TEST(FilterLinesTest, HeadLargerThanLineCount) {
  // head:10 on 3 lines → all 3 lines kept
  json filters = json::array({{{"head", 10}}});
  EXPECT_EQ(ai::filter_lines("a\nb\nc", filters), "a\nb\nc");
}

TEST(FilterLinesTest, HeadNegativeIgnored) {
  json filters = json::array({{{"head", -1}}});
  EXPECT_EQ(ai::filter_lines("a\nb\nc", filters), "a\nb\nc");
}

TEST(FilterLinesTest, HeadOnSingleLine) {
  json filters = json::array({{{"head", 1}}});
  EXPECT_EQ(ai::filter_lines("hello", filters), "hello");
}

TEST(FilterLinesTest, HeadOnTrailingNewlineText) {
  json filters = json::array({{{"head", 1}}});
  EXPECT_EQ(ai::filter_lines("a\nb\nc\n", filters), "a");
}

// ── tail filter ────────────────────────────────────────────────────────────

TEST(FilterLinesTest, TailBasic) {
  json filters = json::array({{{"tail", 2}}});
  EXPECT_EQ(ai::filter_lines("line1\nline2\nline3\nline4\nline5", filters),
            "line4\nline5");
}

TEST(FilterLinesTest, TailZero) {
  json filters = json::array({{{"tail", 0}}});
  EXPECT_EQ(ai::filter_lines("line1\nline2\nline3", filters), "");
}

TEST(FilterLinesTest, TailExactCount) {
  json filters = json::array({{{"tail", 3}}});
  EXPECT_EQ(ai::filter_lines("a\nb\nc", filters), "a\nb\nc");
}

TEST(FilterLinesTest, TailLargerThanLineCount) {
  json filters = json::array({{{"tail", 10}}});
  EXPECT_EQ(ai::filter_lines("a\nb\nc", filters), "a\nb\nc");
}

TEST(FilterLinesTest, TailNegativeIgnored) {
  json filters = json::array({{{"tail", -1}}});
  EXPECT_EQ(ai::filter_lines("a\nb\nc", filters), "a\nb\nc");
}

TEST(FilterLinesTest, TailOnSingleLine) {
  json filters = json::array({{{"tail", 1}}});
  EXPECT_EQ(ai::filter_lines("hello", filters), "hello");
}

TEST(FilterLinesTest, TailOnTrailingNewlineText) {
  json filters = json::array({{{"tail", 1}}});
  EXPECT_EQ(ai::filter_lines("a\nb\nc\n", filters), "c");
}

// ── include filter ─────────────────────────────────────────────────────────

TEST(FilterLinesTest, IncludeBasic) {
  json filters = json::array({{{"include", "hello"}}});
  EXPECT_EQ(ai::filter_lines("hello_world\nfoo_bar\nhello_baz", filters),
            "hello_world\nhello_baz");
}

TEST(FilterLinesTest, IncludeRegex) {
  // Keep lines matching a digit pattern
  json filters = json::array({{{"include", "\\d+"}}});
  EXPECT_EQ(ai::filter_lines("abc\n123\nxyz\n456", filters), "123\n456");
}

TEST(FilterLinesTest, IncludeNoMatch) {
  json filters = json::array({{{"include", "NOMATCH"}}});
  EXPECT_EQ(ai::filter_lines("line1\nline2\nline3", filters), "");
}

TEST(FilterLinesTest, IncludeAllMatch) {
  json filters = json::array({{{"include", ".*"}}});
  EXPECT_EQ(ai::filter_lines("a\nb\nc", filters), "a\nb\nc");
}

TEST(FilterLinesTest, IncludeAnchoredRegex) {
  json filters = json::array({{{"include", "^start"}}});
  EXPECT_EQ(ai::filter_lines("start_of_line\nnot_start\nstart_again", filters),
            "start_of_line\nstart_again");
}

// ── exclude filter ─────────────────────────────────────────────────────────

TEST(FilterLinesTest, ExcludeBasic) {
  json filters = json::array({{{"exclude", "debug"}}});
  EXPECT_EQ(ai::filter_lines("keep_me\ndebug_info\nkeep_too", filters),
            "keep_me\nkeep_too");
}

TEST(FilterLinesTest, ExcludeRegex) {
  json filters = json::array({{{"exclude", "^\\s*$"}}});
  // Exclude empty / whitespace-only lines from a 5-line block where lines
  // 2 and 4 are blank
  EXPECT_EQ(ai::filter_lines("a\n\nb\n\nc", filters), "a\nb\nc");
}

TEST(FilterLinesTest, ExcludeNoMatch) {
  json filters = json::array({{{"exclude", "NOMATCH"}}});
  EXPECT_EQ(ai::filter_lines("line1\nline2\nline3", filters),
            "line1\nline2\nline3");
}

TEST(FilterLinesTest, ExcludeAllMatch) {
  json filters = json::array({{{"exclude", ".*"}}});
  EXPECT_EQ(ai::filter_lines("a\nb\nc", filters), "");
}

// ── Combined / chained filters ─────────────────────────────────────────────

TEST(FilterLinesTest, ExcludeThenInclude) {
  // First exclude lines with "debug", then keep only lines with "keep"
  json filters = json::array({{{"exclude", "debug"}}, {{"include", "keep"}}});
  EXPECT_EQ(
      ai::filter_lines("keep_me\ndebug_stuff\nkeep_too\ndebug_more", filters),
      "keep_me\nkeep_too");
}

TEST(FilterLinesTest, HeadThenInclude) {
  // Take first 3 lines, then keep only those matching "bar"
  json filters = json::array({{{"head", 3}}, {{"include", "bar"}}});
  EXPECT_EQ(ai::filter_lines("foo1\nbar1\nbar2\nfoo2\nbar3", filters),
            "bar1\nbar2");
}

TEST(FilterLinesTest, IncludeThenTail) {
  // Keep lines with "err", then take last 2
  json filters = json::array({{{"include", "err"}}, {{"tail", 2}}});
  EXPECT_EQ(ai::filter_lines("info1\nerr1\ninfo2\nerr2\nerr3\ninfo3", filters),
            "err2\nerr3");
}

TEST(FilterLinesTest, TailThenHead) {
  // Take last 5 lines, then first 2 of those
  json filters = json::array({{{"tail", 5}}, {{"head", 2}}});
  EXPECT_EQ(ai::filter_lines("1\n2\n3\n4\n5\n6\n7\n8\n9\n10", filters), "6\n7");
}

TEST(FilterLinesTest, IncludeThenExclude) {
  // First keep "log:" lines, then exclude "DEBUG"
  json filters = json::array({{{"include", "log:"}}, {{"exclude", "DEBUG"}}});
  EXPECT_EQ(
      ai::filter_lines("log:INFO\nDEBUG:xyz\nlog:DEBUG\nlog:WARN", filters),
      "log:INFO\nlog:WARN");
}

TEST(FilterLinesTest, MultipleFiltersEmptyResult) {
  json filters = json::array({{{"head", 2}}, {{"exclude", "."}}});
  EXPECT_EQ(ai::filter_lines("a\nb\nc\nd", filters), "");
}

// ── Non-object filter entries skipped ──────────────────────────────────────

TEST(FilterLinesTest, NonObjectFilterSkipped) {
  // Non-object entries in the filters array are ignored
  json filters = json::array({"not_an_object", {{"head", 1}}});
  EXPECT_EQ(ai::filter_lines("a\nb\nc", filters), "a");
}

TEST(FilterLinesTest, AllNonObjectFiltersPassthrough) {
  json filters = json::array({"skip1", 42, true});
  EXPECT_EQ(ai::filter_lines("a\nb\nc", filters), "a\nb\nc");
}

// ── Invalid / wrong-type filter values ─────────────────────────────────────

TEST(FilterLinesTest, HeadWithStringValueIgnored) {
  json filters = json::array({{{"head", "not_a_number"}}});
  EXPECT_EQ(ai::filter_lines("a\nb\nc", filters), "a\nb\nc");
}

TEST(FilterLinesTest, TailWithStringValueIgnored) {
  json filters = json::array({{{"tail", "not_a_number"}}});
  EXPECT_EQ(ai::filter_lines("a\nb\nc", filters), "a\nb\nc");
}

TEST(FilterLinesTest, IncludeWithIntegerValueIgnored) {
  json filters = json::array({{{"include", 42}}});
  EXPECT_EQ(ai::filter_lines("a\nb\nc", filters), "a\nb\nc");
}

TEST(FilterLinesTest, ExcludeWithIntegerValueIgnored) {
  json filters = json::array({{{"exclude", 42}}});
  EXPECT_EQ(ai::filter_lines("a\nb\nc", filters), "a\nb\nc");
}

TEST(FilterLinesTest, UnknownFilterKeyIgnored) {
  json filters = json::array({{{"unknown_key", "value"}}});
  EXPECT_EQ(ai::filter_lines("a\nb\nc", filters), "a\nb\nc");
}

// ── Special regex characters in text ───────────────────────────────────────

TEST(FilterLinesTest, IncludeSpecialRegexCharsInText) {
  // The text contains characters that are special in regex: ( ) [ ] . * +
  // The include pattern should match the literal text
  json filters = json::array({{{"include", "foo\\(bar\\)"}}});
  EXPECT_EQ(ai::filter_lines("foo(bar)\nbaz\nqux", filters), "foo(bar)");
}

TEST(FilterLinesTest, ExcludeLiteralDot) {
  // Exclude lines containing literal dot followed by "cpp"
  json filters = json::array({{{"exclude", "\\.cpp"}}});
  EXPECT_EQ(ai::filter_lines("main.cpp\nmain.h\nutils.cpp\nutils.h", filters),
            "main.h\nutils.h");
}

// ── Edge case: empty lines vector after head/tail ──────────────────────────

TEST(FilterLinesTest, HeadZeroThenTailOnEmpty) {
  // head:0 empties the vector, tail operates on empty result
  json filters = json::array({{{"head", 0}}, {{"tail", 1}}});
  EXPECT_EQ(ai::filter_lines("a\nb\nc", filters), "");
}

TEST(FilterLinesTest, TailZeroThenHeadOnEmpty) {
  json filters = json::array({{{"tail", 0}}, {{"head", 1}}});
  EXPECT_EQ(ai::filter_lines("a\nb\nc", filters), "");
}

// ── Single filter object with both include and exclude ─────────────────────
// (exclude takes priority due to if/else-if ordering)

TEST(FilterLinesTest, ExcludePriorityOverInclude) {
  // When a filter object has both "exclude" and "include", "exclude" wins
  // due to if/else-if ordering in filter_lines implementation
  json filter;
  filter["exclude"] = "aaa";
  filter["include"] = "bbb";
  json filters = json::array({filter});
  // Lines containing "aaa" are excluded; "include" is not evaluated
  EXPECT_EQ(ai::filter_lines("aaa\nbbb\nccc", filters), "bbb\nccc");
}
