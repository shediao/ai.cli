#include <gtest/gtest.h>

#include <string>
#include <string_view>

#include "ai/matchglob.h"

using namespace std::literals;

// =============================================================================
// matchglob – narrow string (std::string_view)
// =============================================================================

// ── Exact match (no wildcards) ──────────────────────────────────────────────

TEST(MatchGlobTest, ExactMatch) {
  EXPECT_TRUE(ai::matchglob("hello", "hello", false));
  EXPECT_FALSE(ai::matchglob("hello", "world", false));
}

TEST(MatchGlobTest, ExactMatchEmpty) {
  EXPECT_TRUE(ai::matchglob("", "", false));
  EXPECT_FALSE(ai::matchglob("", "a", false));
  EXPECT_FALSE(ai::matchglob("a", "", false));
}

// ── '?' wildcard ────────────────────────────────────────────────────────────

TEST(MatchGlobTest, QuestionMarkSingle) {
  EXPECT_TRUE(ai::matchglob("?", "a", false));
  EXPECT_TRUE(ai::matchglob("?", "Z", false));
  EXPECT_TRUE(ai::matchglob("?", "0", false));
  EXPECT_FALSE(ai::matchglob("?", "", false));
  EXPECT_FALSE(ai::matchglob("?", "ab", false));
}

TEST(MatchGlobTest, QuestionMarkMultiple) {
  EXPECT_TRUE(ai::matchglob("???", "abc", false));
  EXPECT_TRUE(ai::matchglob("???", "123", false));
  EXPECT_FALSE(ai::matchglob("???", "ab", false));
  EXPECT_FALSE(ai::matchglob("???", "abcd", false));
}

TEST(MatchGlobTest, QuestionMarkMixed) {
  EXPECT_TRUE(ai::matchglob("h?llo", "hello", false));
  EXPECT_TRUE(ai::matchglob("h?llo", "hallo", false));
  EXPECT_FALSE(ai::matchglob("h?llo", "hllo", false));
  EXPECT_FALSE(ai::matchglob("h?llo", "heello", false));
}

// ── '*' wildcard ────────────────────────────────────────────────────────────

TEST(MatchGlobTest, StarOnly) {
  EXPECT_TRUE(ai::matchglob("*", "", false));
  EXPECT_TRUE(ai::matchglob("*", "a", false));
  EXPECT_TRUE(ai::matchglob("*", "hello", false));
  EXPECT_TRUE(ai::matchglob("*", "hello world", false));
}

TEST(MatchGlobTest, StarAtBeginning) {
  EXPECT_TRUE(ai::matchglob("*.txt", "file.txt", false));
  EXPECT_TRUE(ai::matchglob("*.txt", ".txt", false));
  EXPECT_FALSE(ai::matchglob("*.txt", "file.csv", false));
}

TEST(MatchGlobTest, StarAtEnd) {
  EXPECT_TRUE(ai::matchglob("file*", "file", false));
  EXPECT_TRUE(ai::matchglob("file*", "file.txt", false));
  EXPECT_TRUE(ai::matchglob("file*", "file123", false));
  EXPECT_FALSE(ai::matchglob("file*", "fail", false));
}

TEST(MatchGlobTest, StarInMiddle) {
  EXPECT_TRUE(ai::matchglob("a*c", "abc", false));
  EXPECT_TRUE(ai::matchglob("a*c", "ac", false));
  EXPECT_TRUE(ai::matchglob("a*c", "aXYZc", false));
  EXPECT_FALSE(ai::matchglob("a*c", "ab", false));
  EXPECT_FALSE(ai::matchglob("a*c", "bc", false));
}

TEST(MatchGlobTest, MultipleStars) {
  EXPECT_TRUE(ai::matchglob("a*b*c", "abc", false));
  EXPECT_TRUE(ai::matchglob("a*b*c", "aXbYc", false));
  // "ac" has no 'b', so a*b*c does not match
  EXPECT_FALSE(ai::matchglob("a*b*c", "ac", false));
  EXPECT_FALSE(ai::matchglob("a*b*c", "abd", false));
}

TEST(MatchGlobTest, ConsecutiveStars) {
  // Consecutive '*' should be collapsed into one
  EXPECT_TRUE(ai::matchglob("***", "", false));
  EXPECT_TRUE(ai::matchglob("***", "abc", false));
  EXPECT_TRUE(ai::matchglob("a***b", "ab", false));
  EXPECT_TRUE(ai::matchglob("a***b", "aXYZb", false));
  EXPECT_FALSE(ai::matchglob("a***b", "ac", false));
}

// ── '?' and '*' combined ────────────────────────────────────────────────────

TEST(MatchGlobTest, StarQuestionMark) {
  // *? means at least one character
  EXPECT_TRUE(ai::matchglob("*?", "a", false));
  EXPECT_TRUE(ai::matchglob("*?", "ab", false));
  EXPECT_TRUE(ai::matchglob("*?", "abc", false));
  EXPECT_FALSE(ai::matchglob("*?", "", false));
}

TEST(MatchGlobTest, QuestionMarkStar) {
  // ?* means at least one character (same as *?)
  EXPECT_TRUE(ai::matchglob("?*", "a", false));
  EXPECT_TRUE(ai::matchglob("?*", "ab", false));
  EXPECT_FALSE(ai::matchglob("?*", "", false));
}

TEST(MatchGlobTest, ComplexWildcardMix) {
  EXPECT_TRUE(ai::matchglob("?*?", "ab", false));
  EXPECT_TRUE(ai::matchglob("?*?", "abc", false));
  EXPECT_TRUE(ai::matchglob("?*?", "abcd", false));
  EXPECT_FALSE(ai::matchglob("?*?", "a", false));
  EXPECT_FALSE(ai::matchglob("?*?", "", false));

  // More complex: prefix + wildcards + suffix
  EXPECT_TRUE(ai::matchglob("f?*o", "foo", false));
  EXPECT_TRUE(ai::matchglob("f?*o", "fao", false));
  EXPECT_TRUE(ai::matchglob("f?*o", "fabo", false));
  EXPECT_FALSE(ai::matchglob("f?*o", "fo", false));
}

TEST(MatchGlobTest, StarQuestionMarkStar) {
  EXPECT_TRUE(ai::matchglob("*?*", "a", false));
  EXPECT_TRUE(ai::matchglob("*?*", "ab", false));
  EXPECT_FALSE(ai::matchglob("*?*", "", false));
}

// ── Path separator handling ─────────────────────────────────────────────────

TEST(MatchGlobTest, StarDoesNotCrossSlash) {
  // '*' should not match across '/' or '\'
  EXPECT_FALSE(ai::matchglob("*", "a/b", false));
  EXPECT_FALSE(ai::matchglob("*", "a\\b", false));
  EXPECT_FALSE(ai::matchglob("a*b", "a/b", false));
  EXPECT_FALSE(ai::matchglob("a*b", "a\\b", false));
}

TEST(MatchGlobTest, StarDoesNotCrossMixedSeparators) {
  EXPECT_FALSE(ai::matchglob("dir/*", "dir/sub/file", false));
}

TEST(MatchGlobTest, SlashBackslashEquivalenceInChar) {
  // '/' in pattern matches '\' in string and vice versa
  EXPECT_TRUE(ai::matchglob("a/b", "a/b", false));
  EXPECT_TRUE(ai::matchglob("a/b", "a\\b", false));
  EXPECT_TRUE(ai::matchglob("a\\b", "a/b", false));
  EXPECT_TRUE(ai::matchglob("a\\b", "a\\b", false));
}

TEST(MatchGlobTest, SlashBackslashEquivalenceWithStar) {
  // Pattern "a/*/c" against "a/b/c" and "a\\b\\c"
  EXPECT_TRUE(ai::matchglob("a/*/c", "a/b/c", false));
  EXPECT_TRUE(ai::matchglob("a/*/c", "a\\b\\c", false));
  EXPECT_TRUE(ai::matchglob("a\\*\\c", "a/b/c", false));
  EXPECT_TRUE(ai::matchglob("a\\*\\c", "a\\b\\c", false));
}

TEST(MatchGlobTest, StarStopsAtBothSeparators) {
  // On all platforms, '*' stops at both '/' and '\'
  EXPECT_FALSE(ai::matchglob("*.txt", "dir/file.txt", false));
  EXPECT_FALSE(ai::matchglob("*.txt", "dir\\file.txt", false));
  EXPECT_FALSE(ai::matchglob("dir/*.txt", "dir/sub/file.txt", false));
}

// ── Case sensitivity ────────────────────────────────────────────────────────

TEST(MatchGlobTest, CaseSensitive) {
  EXPECT_TRUE(ai::matchglob("Hello", "Hello", false));
  EXPECT_FALSE(ai::matchglob("Hello", "hello", false));
  EXPECT_FALSE(ai::matchglob("Hello", "HELLO", false));
  EXPECT_FALSE(ai::matchglob("hello", "HELLO", false));
}

TEST(MatchGlobTest, CaseInsensitive) {
  EXPECT_TRUE(ai::matchglob("Hello", "Hello", true));
  EXPECT_TRUE(ai::matchglob("Hello", "hello", true));
  EXPECT_TRUE(ai::matchglob("Hello", "HELLO", true));
  EXPECT_TRUE(ai::matchglob("hello", "HELLO", true));
  EXPECT_TRUE(ai::matchglob("HELLO", "hello", true));
}

TEST(MatchGlobTest, CaseInsensitiveWithWildcards) {
  EXPECT_TRUE(ai::matchglob("H*O", "hello", true));
  EXPECT_TRUE(ai::matchglob("h?llo", "HELLO", true));
  EXPECT_FALSE(ai::matchglob("H*O", "hello", false));
}

TEST(MatchGlobTest, CaseInsensitiveNonAlpha) {
  // Case-insensitive matching should not affect non-alpha characters
  EXPECT_TRUE(ai::matchglob("123", "123", true));
  EXPECT_TRUE(ai::matchglob("file_1", "FILE_1", true));
  EXPECT_FALSE(ai::matchglob("file_1", "FILE_2", true));
}

// ── Edge cases ──────────────────────────────────────────────────────────────

TEST(MatchGlobTest, SingleCharacter) {
  EXPECT_TRUE(ai::matchglob("a", "a", false));
  EXPECT_FALSE(ai::matchglob("a", "b", false));
  EXPECT_FALSE(ai::matchglob("a", "A", false));
  EXPECT_TRUE(ai::matchglob("a", "A", true));
}

TEST(MatchGlobTest, PatternLongerThanString) {
  EXPECT_FALSE(ai::matchglob("hello!", "hello", false));
}

TEST(MatchGlobTest, StringLongerThanPattern) {
  EXPECT_FALSE(ai::matchglob("hello", "hello!", false));
}

TEST(MatchGlobTest, SpecialCharacters) {
  EXPECT_TRUE(ai::matchglob("file.txt", "file.txt", false));
  EXPECT_TRUE(ai::matchglob("file?txt", "file.txt", false));
  EXPECT_TRUE(ai::matchglob("file*txt", "file.txt", false));
  EXPECT_TRUE(ai::matchglob("file.*", "file.txt", false));
  EXPECT_TRUE(ai::matchglob("file.*", "file.", false));
}

TEST(MatchGlobTest, Spaces) {
  EXPECT_TRUE(ai::matchglob("hello world", "hello world", false));
  EXPECT_TRUE(ai::matchglob("hello*", "hello world", false));
  EXPECT_TRUE(ai::matchglob("*world", "hello world", false));
  EXPECT_TRUE(ai::matchglob("h*d", "hello world", false));
}

TEST(MatchGlobTest, UnderscoreAndHyphen) {
  EXPECT_TRUE(ai::matchglob("file_name", "file_name", false));
  EXPECT_TRUE(ai::matchglob("file-name", "file-name", false));
  EXPECT_TRUE(ai::matchglob("file?name", "file_name", false));
  EXPECT_TRUE(ai::matchglob("file?name", "file-name", false));
  EXPECT_TRUE(ai::matchglob("file*name", "file_some_name", false));
}

TEST(MatchGlobTest, AllWildcardsPattern) {
  // Pure wildcard patterns
  EXPECT_TRUE(ai::matchglob("?*?*?", "abc", false));
  EXPECT_TRUE(ai::matchglob("?*?*?", "abcde", false));
  EXPECT_FALSE(ai::matchglob("?*?*?", "ab", false));
  EXPECT_FALSE(ai::matchglob("?*?*?", "", false));
}

TEST(MatchGlobTest, NonAsciiUtf8) {
  // UTF-8 multi-byte characters – Note: matchglob operates on bytes, not
  // Unicode code points. A '?' matches exactly one byte, and a Chinese
  // character in UTF-8 is typically 3 bytes.
  EXPECT_TRUE(ai::matchglob("你好", "你好", false));
  EXPECT_FALSE(ai::matchglob("你好", "世界", false));
  // '*' wildcards work across multi-byte boundaries
  EXPECT_TRUE(ai::matchglob("你*", "你好世界", false));
  EXPECT_TRUE(ai::matchglob("*世界", "你好世界", false));
  EXPECT_TRUE(ai::matchglob("你*界", "你好世界", false));
  // Exact byte-level: pattern has 你(3B)+*(any)+好(3B), string has
  // 你(3B)+好(3B)+吗(3B). The * matches "好" leaving no bytes for the
  // trailing 好 to match.
  EXPECT_FALSE(ai::matchglob("你*好", "你好吗", false));
}

TEST(MatchGlobTest, LongStrings) {
  std::string long_str(1000, 'a');
  EXPECT_TRUE(ai::matchglob("*", long_str, false));
  EXPECT_TRUE(ai::matchglob(long_str, long_str, false));
  EXPECT_TRUE(ai::matchglob("a*", long_str, false));
  EXPECT_TRUE(ai::matchglob("*a", long_str, false));

  // Long string with pattern that requires backtracking
  std::string pattern = "a*b"s + std::string(500, 'c');
  EXPECT_TRUE(ai::matchglob("a*b", "a" + std::string(500, 'x') + "b", false));
  EXPECT_FALSE(ai::matchglob("a*b", "a" + std::string(500, 'x') + "c", false));
}

TEST(MatchGlobTest, StarAtBoundaries) {
  // Star at very start and very end
  EXPECT_TRUE(ai::matchglob("*a*", "a", false));
  EXPECT_TRUE(ai::matchglob("*a*", "bab", false));
  EXPECT_FALSE(ai::matchglob("*a*", "bbb", false));  // no 'a'
  EXPECT_TRUE(ai::matchglob("*a*", "aa", false));
  // Pattern that is all stars matches everything
  EXPECT_TRUE(ai::matchglob("*", "", false));
  EXPECT_TRUE(ai::matchglob("*", "anything", false));
}

TEST(MatchGlobTest, NumbersAndUnderscores) {
  EXPECT_TRUE(ai::matchglob("test_001", "test_001", false));
  EXPECT_TRUE(ai::matchglob("test_???", "test_001", false));
  EXPECT_TRUE(ai::matchglob("test_*", "test_001", false));
  EXPECT_TRUE(ai::matchglob("test_*", "test_", false));
}

TEST(MatchGlobTest, DotFiles) {
  EXPECT_TRUE(ai::matchglob(".*", ".gitignore", false));
  EXPECT_TRUE(ai::matchglob(".*", ".", false));
  EXPECT_FALSE(ai::matchglob(".*", "file", false));
  EXPECT_TRUE(ai::matchglob(".?*", ".a", false));
  EXPECT_TRUE(ai::matchglob(".?*", ".gitignore", false));
  EXPECT_FALSE(ai::matchglob(".?*", ".", false));
}

// ── Tricky backtracking cases ───────────────────────────────────────────────

TEST(MatchGlobTest, BacktrackingStarThenChar) {
  // Pattern "a*bc" against "abcbc" - star should match "bc" leaving "bc" for
  // literal
  EXPECT_TRUE(ai::matchglob("a*bc", "abcbc", false));
  EXPECT_TRUE(ai::matchglob("a*bc", "abc", false));
  EXPECT_TRUE(ai::matchglob("a*bc", "aXYZbc", false));
  EXPECT_FALSE(ai::matchglob("a*bc", "abcbd", false));
}

TEST(MatchGlobTest, BacktrackingMultipleStars) {
  // Multiple stars requiring correct backtracking
  EXPECT_TRUE(ai::matchglob("*a*b*", "xxaybzz", false));
  EXPECT_TRUE(ai::matchglob("*a*b*", "ab", false));
  EXPECT_TRUE(ai::matchglob("*a*b*", "bab", false));
  // "xyza" has no 'b' after 'a'
  EXPECT_FALSE(ai::matchglob("*a*b*", "xyza", false));
  EXPECT_FALSE(ai::matchglob("*a*b*", "xyz", false));
  // Backtracking: first * matches "xxa", 'a' matches 'a', second * matches
  // "", then 'b' matches
  EXPECT_TRUE(ai::matchglob("*a*b", "xxab", false));
}

TEST(MatchGlobTest, GreedyStarBacktrack) {
  // The first * greedily matches, then needs to backtrack
  EXPECT_TRUE(ai::matchglob("*abc*def", "abc123def", false));
  EXPECT_TRUE(ai::matchglob("*abc*def", "xyzabc123def", false));
  EXPECT_TRUE(ai::matchglob("*abc*def", "abcabcdefdef", false));
}

// =============================================================================
// matchglob – wide string (std::wstring_view)
// =============================================================================

TEST(MatchGlobWideTest, ExactMatch) {
  EXPECT_TRUE(ai::matchglob(L"hello", L"hello", false));
  EXPECT_FALSE(ai::matchglob(L"hello", L"world", false));
}

TEST(MatchGlobWideTest, Empty) {
  EXPECT_TRUE(ai::matchglob(L"", L"", false));
  EXPECT_FALSE(ai::matchglob(L"", L"a", false));
  EXPECT_FALSE(ai::matchglob(L"a", L"", false));
}

TEST(MatchGlobWideTest, QuestionMark) {
  EXPECT_TRUE(ai::matchglob(L"?", L"a", false));
  EXPECT_TRUE(ai::matchglob(L"???", L"abc", false));
  EXPECT_FALSE(ai::matchglob(L"?", L"", false));
  EXPECT_FALSE(ai::matchglob(L"?", L"ab", false));
}

TEST(MatchGlobWideTest, Star) {
  EXPECT_TRUE(ai::matchglob(L"*", L"", false));
  EXPECT_TRUE(ai::matchglob(L"*", L"hello", false));
  EXPECT_TRUE(ai::matchglob(L"a*c", L"abc", false));
  EXPECT_TRUE(ai::matchglob(L"a*c", L"ac", false));
  EXPECT_TRUE(ai::matchglob(L"a*c", L"aXYZc", false));
}

TEST(MatchGlobWideTest, StarQuestionMark) {
  EXPECT_TRUE(ai::matchglob(L"*?", L"a", false));
  EXPECT_TRUE(ai::matchglob(L"*?", L"ab", false));
  EXPECT_FALSE(ai::matchglob(L"*?", L"", false));
}

TEST(MatchGlobWideTest, QuestionMarkStar) {
  EXPECT_TRUE(ai::matchglob(L"?*", L"a", false));
  EXPECT_TRUE(ai::matchglob(L"?*", L"ab", false));
  EXPECT_FALSE(ai::matchglob(L"?*", L"", false));
}

TEST(MatchGlobWideTest, PathSeparators) {
  EXPECT_FALSE(ai::matchglob(L"*", L"a/b", false));
  EXPECT_FALSE(ai::matchglob(L"*", L"a\\b", false));
  EXPECT_TRUE(ai::matchglob(L"a/b", L"a\\b", false));
  EXPECT_TRUE(ai::matchglob(L"a\\b", L"a/b", false));
}

TEST(MatchGlobWideTest, CaseInsensitive) {
  EXPECT_TRUE(ai::matchglob(L"Hello", L"hello", true));
  EXPECT_TRUE(ai::matchglob(L"HELLO", L"hello", true));
  EXPECT_FALSE(ai::matchglob(L"Hello", L"hello", false));
}

TEST(MatchGlobWideTest, CaseInsensitiveMixed) {
  EXPECT_TRUE(ai::matchglob(L"H*O", L"hello", true));
  EXPECT_TRUE(ai::matchglob(L"h?llo", L"HELLO", true));
  EXPECT_FALSE(ai::matchglob(L"H*O", L"hello", false));
}

TEST(MatchGlobWideTest, NonAscii) {
  // Wide strings: each wchar_t is one character unit, so '?' matches exactly
  // one wchar_t.
  EXPECT_TRUE(ai::matchglob(L"你好", L"你好", false));
  EXPECT_FALSE(ai::matchglob(L"你好", L"世界", false));
  EXPECT_TRUE(ai::matchglob(L"你*", L"你好世界", false));
  // 你?好 vs 你好吗: 你=你 ✓, ?=好 ✓, 好≠吗 ✗
  EXPECT_FALSE(ai::matchglob(L"你?好", L"你好吗", false));
  // Correct: ? matches exactly one wchar_t
  EXPECT_TRUE(ai::matchglob(L"你?", L"你好", false));
  EXPECT_TRUE(ai::matchglob(L"?好", L"你好", false));
  EXPECT_TRUE(ai::matchglob(L"你?好", L"你X好", false));
}

TEST(MatchGlobWideTest, ConsecutiveStars) {
  EXPECT_TRUE(ai::matchglob(L"***", L"abc", false));
  EXPECT_TRUE(ai::matchglob(L"***", L"", false));
  EXPECT_TRUE(ai::matchglob(L"a***b", L"ab", false));
}
