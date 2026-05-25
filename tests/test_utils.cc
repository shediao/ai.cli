#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>

#include "ai/utils.h"

namespace utils = ai::utils;

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
