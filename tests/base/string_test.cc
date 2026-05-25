#include "base/string.h"

#include <gtest/gtest.h>
// =============================================================================
// split
// =============================================================================

TEST(SplitTest, NormalSplit) {
  auto result = ai::base::split("a,b,c", ',');
  ASSERT_EQ(result.size(), 3u);
  EXPECT_EQ(result[0], "a");
  EXPECT_EQ(result[1], "b");
  EXPECT_EQ(result[2], "c");
}

TEST(SplitTest, EmptyString) {
  auto result = ai::base::split("", ',');
  EXPECT_TRUE(result.empty());
}

TEST(SplitTest, NoDelimiter) {
  auto result = ai::base::split("hello", ',');
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result[0], "hello");
}

TEST(SplitTest, SingleChar) {
  auto result = ai::base::split("x", ',');
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result[0], "x");
}

TEST(SplitTest, DelimiterAtStart) {
  auto result = ai::base::split(",a,b", ',');
  ASSERT_EQ(result.size(), 2u);
  EXPECT_EQ(result[0], "a");
  EXPECT_EQ(result[1], "b");
}

TEST(SplitTest, DelimiterAtEnd) {
  auto result = ai::base::split("a,b,", ',');
  ASSERT_EQ(result.size(), 2u);
  EXPECT_EQ(result[0], "a");
  EXPECT_EQ(result[1], "b");
}

TEST(SplitTest, ConsecutiveDelimiters) {
  auto result = ai::base::split("a,,b", ',');
  ASSERT_EQ(result.size(), 2u);
  EXPECT_EQ(result[0], "a");
  EXPECT_EQ(result[1], "b");
}

TEST(SplitTest, MultipleConsecutiveDelimiters) {
  auto result = ai::base::split("a,,,b", ',');
  ASSERT_EQ(result.size(), 2u);
  EXPECT_EQ(result[0], "a");
  EXPECT_EQ(result[1], "b");
}

TEST(SplitTest, OnlyDelimiters) {
  auto result = ai::base::split(",,,", ',');
  EXPECT_TRUE(result.empty());
}

TEST(SplitTest, SingleToken) {
  auto result = ai::base::split("onlyone", ',');
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result[0], "onlyone");
}

TEST(SplitTest, TrimsSpacesAroundTokens) {
  auto result = ai::base::split(" a , b , c ", ',');
  ASSERT_EQ(result.size(), 3u);
  EXPECT_EQ(result[0], "a");
  EXPECT_EQ(result[1], "b");
  EXPECT_EQ(result[2], "c");
}

TEST(SplitTest, TrimsTabsAroundTokens) {
  auto result = ai::base::split("\ta\t,\tb\t,\tc\t", ',');
  ASSERT_EQ(result.size(), 3u);
  EXPECT_EQ(result[0], "a");
  EXPECT_EQ(result[1], "b");
  EXPECT_EQ(result[2], "c");
}

TEST(SplitTest, MixedWhitespaceAndEmpty) {
  auto result = ai::base::split(" a ,  , c ", ',');
  ASSERT_EQ(result.size(), 2u);
  EXPECT_EQ(result[0], "a");
  EXPECT_EQ(result[1], "c");
}

TEST(SplitTest, PreservesInternalSpaces) {
  auto result = ai::base::split("hello world,foo bar", ',');
  ASSERT_EQ(result.size(), 2u);
  EXPECT_EQ(result[0], "hello world");
  EXPECT_EQ(result[1], "foo bar");
}

TEST(SplitTest, AllSpacesTokens) {
  auto result = ai::base::split("   ,   ,   ", ',');
  EXPECT_TRUE(result.empty());
}

TEST(SplitTest, WideString) {
  auto result = ai::base::split(std::wstring(L"a,b,c"), L',');
  ASSERT_EQ(result.size(), 3u);
  EXPECT_EQ(result[0], L"a");
  EXPECT_EQ(result[1], L"b");
  EXPECT_EQ(result[2], L"c");
}

TEST(SplitTest, WideStringEmpty) {
  auto result = ai::base::split(std::wstring(L""), L',');
  EXPECT_TRUE(result.empty());
}

TEST(SplitTest, WideStringNoDelimiter) {
  auto result = ai::base::split(std::wstring(L"hello"), L',');
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result[0], L"hello");
}

// =============================================================================
// utf8_truncate
// =============================================================================

TEST(Utf8TruncateTest, EmptyString) {
  auto result = ai::base::utf8_truncate("", 10);
  EXPECT_EQ(result, "");
}

TEST(Utf8TruncateTest, EmptyStringZeroLimit) {
  auto result = ai::base::utf8_truncate("", 0);
  EXPECT_EQ(result, "");
}

TEST(Utf8TruncateTest, StringShorterThanLimit) {
  auto result = ai::base::utf8_truncate("hello", 10);
  EXPECT_EQ(result, "hello");
}

TEST(Utf8TruncateTest, StringExactlyAtLimit) {
  auto result = ai::base::utf8_truncate("hello", 5);
  EXPECT_EQ(result, "hello");
}

TEST(Utf8TruncateTest, AsciiTruncation) {
  auto result = ai::base::utf8_truncate("hello world", 5);
  EXPECT_EQ(result, "hello");
}

TEST(Utf8TruncateTest, ZeroLimitNonEmpty) {
  auto result = ai::base::utf8_truncate("hello", 0);
  EXPECT_EQ(result, "");
}

TEST(Utf8TruncateTest, SingleAsciiChar) {
  auto result = ai::base::utf8_truncate("a", 1);
  EXPECT_EQ(result, "a");
}

TEST(Utf8TruncateTest, TwoByteUtf8Char) {
  // '©' (copyright sign) is 2 bytes in UTF-8: 0xC2 0xA9
  std::string input = "\xc2\xa9\xc2\xa9\xc2\xa9";  // "©©©" — 6 bytes, 3 chars
  auto result = ai::base::utf8_truncate(input, 2);
  EXPECT_EQ(result, "\xc2\xa9\xc2\xa9");  // "©©" — 4 bytes, 2 chars
}

TEST(Utf8TruncateTest, ThreeByteUtf8Char) {
  // CJK character '中' is 3 bytes in UTF-8: 0xE4 0xB8 0xAD
  std::string input =
      "\xe4\xb8\xad\xe6\x96\x87\xe6\xb5\x8b\xe8\xaf\x95";  // "中文测试" — 12
                                                           // bytes, 4 chars
  auto result = ai::base::utf8_truncate(input, 2);
  EXPECT_EQ(result,
            "\xe4\xb8\xad\xe6\x96\x87");  // "中文" — 6 bytes, 2 chars
}

TEST(Utf8TruncateTest, FourByteUtf8Char) {
  // Emoji '😀' is 4 bytes in UTF-8: 0xF0 0x9F 0x98 0x80
  std::string input =
      "\xf0\x9f\x98\x80\xf0\x9f\x98\x81\xf0\x9f\x98\x82";  // "😀😁😂" — 12
                                                           // bytes, 3 chars
  auto result = ai::base::utf8_truncate(input, 1);
  EXPECT_EQ(result, "\xf0\x9f\x98\x80");  // "😀" — 4 bytes, 1 char
}

TEST(Utf8TruncateTest, MixedAsciiAndUtf8) {
  // "hi中文" = "hi"(2 bytes) + "中文"(6 bytes) = 8 bytes, 4 chars
  std::string input = "hi\xe4\xb8\xad\xe6\x96\x87";
  auto result = ai::base::utf8_truncate(input, 3);
  EXPECT_EQ(result, "hi\xe4\xb8\xad");  // "hi中" — 5 bytes, 3 chars
}

TEST(Utf8TruncateTest, LimitBeyondStringLength) {
  auto result = ai::base::utf8_truncate("abc", 100);
  EXPECT_EQ(result, "abc");
}

TEST(Utf8TruncateTest, LargeTruncation) {
  std::string input(1000, 'x');
  auto result = ai::base::utf8_truncate(input, 500);
  EXPECT_EQ(result, std::string(500, 'x'));
}

TEST(Utf8TruncateTest, ByteSizeCheckAfterTruncation) {
  // After truncating CJK text, the byte size must be valid UTF-8
  // "中文" is 6 bytes, 2 chars
  std::string input = "\xe4\xb8\xad\xe6\x96\x87";
  auto result = ai::base::utf8_truncate(input, 1);
  EXPECT_EQ(result.size(), 3u);  // "中" = 3 bytes
  EXPECT_EQ(result, "\xe4\xb8\xad");
}
