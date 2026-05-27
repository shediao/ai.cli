#include <gtest/gtest.h>

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using json = nlohmann::json;

// Forward declare for direct testing (lives in src/tools/ask_user.cpp)
namespace ai::detail {
std::string ask_user_impl(json const& args);
}

// =============================================================================
// Validation tests (direct call, bypasses enabled() check)
// =============================================================================

TEST(AskUserTest, NotAnObject) {
  json args = json::array();
  std::string result = ai::detail::ask_user_impl(args);
  EXPECT_NE(result.find("expected a JSON object"), std::string::npos);
}

TEST(AskUserTest, MissingQuestion) {
  json args = json::object();
  std::string result = ai::detail::ask_user_impl(args);
  EXPECT_NE(result.find("missing required parameter"), std::string::npos);
  EXPECT_NE(result.find("question"), std::string::npos);
}

TEST(AskUserTest, QuestionNotString) {
  json args = {{"question", 12345}, {"options", json::array({"a", "b"})}};
  std::string result = ai::detail::ask_user_impl(args);
  EXPECT_NE(result.find("\"question\" must be a"), std::string::npos);
}

TEST(AskUserTest, MissingOptions) {
  json args = {{"question", "what?"}};
  std::string result = ai::detail::ask_user_impl(args);
  EXPECT_NE(result.find("missing required parameter"), std::string::npos);
  EXPECT_NE(result.find("options"), std::string::npos);
}

TEST(AskUserTest, OptionsNotArray) {
  json args = {{"question", "what?"}, {"options", "not-an-array"}};
  std::string result = ai::detail::ask_user_impl(args);
  EXPECT_NE(result.find("\"options\" must be an"), std::string::npos);
}

TEST(AskUserTest, EmptyOptions) {
  json args = {{"question", "what?"}, {"options", json::array()}};
  std::string result = ai::detail::ask_user_impl(args);
  EXPECT_NE(result.find("must not be empty"), std::string::npos);
}

TEST(AskUserTest, OptionsWithNonStringElement) {
  json args = {{"question", "pick one"}, {"options", json::array({"a", 42})}};
  std::string result = ai::detail::ask_user_impl(args);
  EXPECT_NE(result.find("all elements in \"options\" must be strings"),
            std::string::npos);
}
