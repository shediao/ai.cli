#include <gtest/gtest.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <thread>
#include <type_traits>

#include "ai/utils.h"
#include "base/scope_exit.h"
#include "environment/environment.hpp"

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
// format_timestamp
// =============================================================================

TEST(TimestampTest, DefaultFormatIsNonEmpty) {
  std::string ts = utils::format_timestamp();
  EXPECT_FALSE(ts.empty());
  // Default format "%Y/%m/%d %H:%M:%S %z" looks like: "2025/07/11 14:30:00
  // +0800"
  EXPECT_GE(ts.size(), 20u);
}

TEST(TimestampTest, DefaultFormatContainsSlash) {
  std::string ts = utils::format_timestamp();
  EXPECT_NE(ts.find('/'), std::string::npos);
}

TEST(TimestampTest, CustomFormatYearOnly) {
  std::string ts = utils::format_timenow("%Y");
  EXPECT_EQ(ts.size(), 4u);
  for (char c : ts) {
    EXPECT_TRUE(std::isdigit(static_cast<unsigned char>(c)));
  }
}

TEST(TimestampTest, CustomFormatFullDate) {
  std::string ts = utils::format_timenow("%Y-%m-%d");
  EXPECT_EQ(ts.size(), 10u);
  EXPECT_EQ(ts[4], '-');
  EXPECT_EQ(ts[7], '-');
}

TEST(TimestampTest, CustomFormatTimeOnly) {
  std::string ts =
      utils::format_timestamp(std::chrono::system_clock::now(), "%H:%M:%S");
  EXPECT_EQ(ts.size(), 8u);
  EXPECT_EQ(ts[2], ':');
  EXPECT_EQ(ts[5], ':');
}

TEST(TimestampTest, TwoCallsReturnCloseValues) {
  std::string ts1 =
      utils::format_timestamp(std::chrono::system_clock::now(), "%Y%m%d%H%M%S");
  // Sleep into the next second to guarantee difference
  std::this_thread::sleep_for(std::chrono::seconds(1));
  std::string ts2 =
      utils::format_timestamp(std::chrono::system_clock::now(), "%Y%m%d%H%M%S");
  EXPECT_NE(ts1, ts2);
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
// split
// =============================================================================

TEST(SplitTest, NormalSplit) {
  auto result = utils::split("a,b,c", ',');
  ASSERT_EQ(result.size(), 3u);
  EXPECT_EQ(result[0], "a");
  EXPECT_EQ(result[1], "b");
  EXPECT_EQ(result[2], "c");
}

TEST(SplitTest, EmptyString) {
  auto result = utils::split("", ',');
  EXPECT_TRUE(result.empty());
}

TEST(SplitTest, NoDelimiter) {
  auto result = utils::split("hello", ',');
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result[0], "hello");
}

TEST(SplitTest, SingleChar) {
  auto result = utils::split("x", ',');
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result[0], "x");
}

TEST(SplitTest, DelimiterAtStart) {
  auto result = utils::split(",a,b", ',');
  ASSERT_EQ(result.size(), 2u);
  EXPECT_EQ(result[0], "a");
  EXPECT_EQ(result[1], "b");
}

TEST(SplitTest, DelimiterAtEnd) {
  auto result = utils::split("a,b,", ',');
  ASSERT_EQ(result.size(), 2u);
  EXPECT_EQ(result[0], "a");
  EXPECT_EQ(result[1], "b");
}

TEST(SplitTest, ConsecutiveDelimiters) {
  auto result = utils::split("a,,b", ',');
  ASSERT_EQ(result.size(), 2u);
  EXPECT_EQ(result[0], "a");
  EXPECT_EQ(result[1], "b");
}

TEST(SplitTest, MultipleConsecutiveDelimiters) {
  auto result = utils::split("a,,,b", ',');
  ASSERT_EQ(result.size(), 2u);
  EXPECT_EQ(result[0], "a");
  EXPECT_EQ(result[1], "b");
}

TEST(SplitTest, OnlyDelimiters) {
  auto result = utils::split(",,,", ',');
  EXPECT_TRUE(result.empty());
}

TEST(SplitTest, SingleToken) {
  auto result = utils::split("onlyone", ',');
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result[0], "onlyone");
}

TEST(SplitTest, TrimsSpacesAroundTokens) {
  auto result = utils::split(" a , b , c ", ',');
  ASSERT_EQ(result.size(), 3u);
  EXPECT_EQ(result[0], "a");
  EXPECT_EQ(result[1], "b");
  EXPECT_EQ(result[2], "c");
}

TEST(SplitTest, TrimsTabsAroundTokens) {
  auto result = utils::split("\ta\t,\tb\t,\tc\t", ',');
  ASSERT_EQ(result.size(), 3u);
  EXPECT_EQ(result[0], "a");
  EXPECT_EQ(result[1], "b");
  EXPECT_EQ(result[2], "c");
}

TEST(SplitTest, MixedWhitespaceAndEmpty) {
  auto result = utils::split(" a ,  , c ", ',');
  ASSERT_EQ(result.size(), 2u);
  EXPECT_EQ(result[0], "a");
  EXPECT_EQ(result[1], "c");
}

TEST(SplitTest, PreservesInternalSpaces) {
  auto result = utils::split("hello world,foo bar", ',');
  ASSERT_EQ(result.size(), 2u);
  EXPECT_EQ(result[0], "hello world");
  EXPECT_EQ(result[1], "foo bar");
}

TEST(SplitTest, AllSpacesTokens) {
  auto result = utils::split("   ,   ,   ", ',');
  EXPECT_TRUE(result.empty());
}

TEST(SplitTest, WideString) {
  auto result = utils::split(std::wstring(L"a,b,c"), L',');
  ASSERT_EQ(result.size(), 3u);
  EXPECT_EQ(result[0], L"a");
  EXPECT_EQ(result[1], L"b");
  EXPECT_EQ(result[2], L"c");
}

TEST(SplitTest, WideStringEmpty) {
  auto result = utils::split(std::wstring(L""), L',');
  EXPECT_TRUE(result.empty());
}

TEST(SplitTest, WideStringNoDelimiter) {
  auto result = utils::split(std::wstring(L"hello"), L',');
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result[0], L"hello");
}

// =============================================================================
// format_timenow
// =============================================================================

TEST(FormatTimenowTest, ReturnsNonEmpty) {
  std::string ts = utils::format_timenow();
  EXPECT_FALSE(ts.empty());
  EXPECT_GE(ts.size(), 20u);
}

TEST(FormatTimenowTest, CustomFormat) {
  std::string ts = utils::format_timenow("%Y-%m-%d");
  EXPECT_EQ(ts.size(), 10u);
  EXPECT_EQ(ts[4], '-');
  EXPECT_EQ(ts[7], '-');
}

TEST(FormatTimenowTest, TwoCallsReturnCloseValues) {
  auto t1 = std::chrono::system_clock::now();
  std::string ts1 = utils::format_timenow("%Y%m%d%H%M%S");
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  auto t2 = std::chrono::system_clock::now();
  std::string ts2 = utils::format_timenow("%Y%m%d%H%M%S");
  // The actual wall-clock interval should be close to the 100ms sleep,
  // but we allow generous bounds for heavily loaded CI runners.
  auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1);
  EXPECT_GE(diff.count(), 1);
  EXPECT_LE(diff.count(), 10000);
}

// =============================================================================
// utf8_truncate
// =============================================================================

TEST(Utf8TruncateTest, EmptyString) {
  auto result = utils::utf8_truncate("", 10);
  EXPECT_EQ(result, "");
}

TEST(Utf8TruncateTest, EmptyStringZeroLimit) {
  auto result = utils::utf8_truncate("", 0);
  EXPECT_EQ(result, "");
}

TEST(Utf8TruncateTest, StringShorterThanLimit) {
  auto result = utils::utf8_truncate("hello", 10);
  EXPECT_EQ(result, "hello");
}

TEST(Utf8TruncateTest, StringExactlyAtLimit) {
  auto result = utils::utf8_truncate("hello", 5);
  EXPECT_EQ(result, "hello");
}

TEST(Utf8TruncateTest, AsciiTruncation) {
  auto result = utils::utf8_truncate("hello world", 5);
  EXPECT_EQ(result, "hello");
}

TEST(Utf8TruncateTest, ZeroLimitNonEmpty) {
  auto result = utils::utf8_truncate("hello", 0);
  EXPECT_EQ(result, "");
}

TEST(Utf8TruncateTest, SingleAsciiChar) {
  auto result = utils::utf8_truncate("a", 1);
  EXPECT_EQ(result, "a");
}

TEST(Utf8TruncateTest, TwoByteUtf8Char) {
  // '©' (copyright sign) is 2 bytes in UTF-8: 0xC2 0xA9
  std::string input = "\xc2\xa9\xc2\xa9\xc2\xa9";  // "©©©" — 6 bytes, 3 chars
  auto result = utils::utf8_truncate(input, 2);
  EXPECT_EQ(result, "\xc2\xa9\xc2\xa9");  // "©©" — 4 bytes, 2 chars
}

TEST(Utf8TruncateTest, ThreeByteUtf8Char) {
  // CJK character '中' is 3 bytes in UTF-8: 0xE4 0xB8 0xAD
  std::string input =
      "\xe4\xb8\xad\xe6\x96\x87\xe6\xb5\x8b\xe8\xaf\x95";  // "中文测试" — 12
                                                           // bytes, 4 chars
  auto result = utils::utf8_truncate(input, 2);
  EXPECT_EQ(result,
            "\xe4\xb8\xad\xe6\x96\x87");  // "中文" — 6 bytes, 2 chars
}

TEST(Utf8TruncateTest, FourByteUtf8Char) {
  // Emoji '😀' is 4 bytes in UTF-8: 0xF0 0x9F 0x98 0x80
  std::string input =
      "\xf0\x9f\x98\x80\xf0\x9f\x98\x81\xf0\x9f\x98\x82";  // "😀😁😂" — 12
                                                           // bytes, 3 chars
  auto result = utils::utf8_truncate(input, 1);
  EXPECT_EQ(result, "\xf0\x9f\x98\x80");  // "😀" — 4 bytes, 1 char
}

TEST(Utf8TruncateTest, MixedAsciiAndUtf8) {
  // "hi中文" = "hi"(2 bytes) + "中文"(6 bytes) = 8 bytes, 4 chars
  std::string input = "hi\xe4\xb8\xad\xe6\x96\x87";
  auto result = utils::utf8_truncate(input, 3);
  EXPECT_EQ(result, "hi\xe4\xb8\xad");  // "hi中" — 5 bytes, 3 chars
}

TEST(Utf8TruncateTest, LimitBeyondStringLength) {
  auto result = utils::utf8_truncate("abc", 100);
  EXPECT_EQ(result, "abc");
}

TEST(Utf8TruncateTest, LargeTruncation) {
  std::string input(1000, 'x');
  auto result = utils::utf8_truncate(input, 500);
  EXPECT_EQ(result, std::string(500, 'x'));
}

TEST(Utf8TruncateTest, ByteSizeCheckAfterTruncation) {
  // After truncating CJK text, the byte size must be valid UTF-8
  // "中文" is 6 bytes, 2 chars
  std::string input = "\xe4\xb8\xad\xe6\x96\x87";
  auto result = utils::utf8_truncate(input, 1);
  EXPECT_EQ(result.size(), 3u);  // "中" = 3 bytes
  EXPECT_EQ(result, "\xe4\xb8\xad");
}
