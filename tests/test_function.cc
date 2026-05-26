#include <gtest/gtest.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "ai/function.h"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

// =============================================================================
// Test helper: concrete Function subclass
// =============================================================================

class TestFunction : public ai::Function {
 public:
  TestFunction(std::string category, std::string name, std::string desc,
               std::string call_result = "", bool enabled = true)
      : cat_(std::move(category)),
        call_result_(std::move(call_result)),
        enabled_(enabled) {
    schema_["name"] = std::move(name);
    schema_["description"] = std::move(desc);
  }

  std::string call(json const& args) override {
    last_args_ = args;
    return call_result_;
  }

  bool enabled() const override { return enabled_; }
  std::string const& category() const override { return cat_; }
  json const& schema() const override { return schema_; }

  json const& last_args() const { return last_args_; }

 private:
  std::string cat_;
  json schema_;
  std::string call_result_;
  bool enabled_;
  mutable json last_args_;
};

// =============================================================================
// Function::name()
// =============================================================================

TEST(FunctionName, ReturnsNameFromSchema) {
  TestFunction f("cat", "my_func", "does things");
  EXPECT_EQ(f.name(), "my_func");
}

TEST(FunctionName, ReturnsEmptyWhenSchemaHasNoName) {
  // schema with no "name" key — empty JSON object
  class NoNameFunc : public ai::Function {
   public:
    std::string call(json const&) override { return ""; }
    std::string const& category() const override { return cat_; }
    json const& schema() const override { return schema_; }

   private:
    std::string cat_ = "cat";
    json schema_ = json::object();
  };

  NoNameFunc f;
  EXPECT_EQ(f.name(), "");
}

// =============================================================================
// Function::description()
// =============================================================================

TEST(FunctionDescription, ReturnsDescriptionFromSchema) {
  TestFunction f("cat", "f", "a helpful description");
  EXPECT_EQ(f.description(), "a helpful description");
}

TEST(FunctionDescription, ReturnsEmptyWhenSchemaHasNoDescription) {
  class NoDescFunc : public ai::Function {
   public:
    std::string call(json const&) override { return ""; }
    std::string const& category() const override { return cat_; }
    json const& schema() const override { return schema_; }

   private:
    std::string cat_ = "cat";
    json schema_ = json::object();
  };

  NoDescFunc f;
  EXPECT_EQ(f.description(), "");
}

// =============================================================================
// Function::enabled()
// =============================================================================

TEST(FunctionEnabled, DefaultReturnsTrue) {
  TestFunction f("cat", "f", "d");
  EXPECT_TRUE(f.enabled());
}

TEST(FunctionEnabled, CanReturnFalse) {
  TestFunction f("cat", "f", "d", "", false);
  EXPECT_FALSE(f.enabled());
}

// =============================================================================
// regist_function
// =============================================================================

TEST(RegistFunction, NullFunctionDoesNotChangeCategories) {
  auto initial = ai::get_categories();
  ai::regist_function(nullptr);
  EXPECT_EQ(ai::get_categories(), initial);
}

TEST(RegistFunction, EnabledFunctionGetsRegistered) {
  auto func = std::make_unique<TestFunction>("reg_cat", "reg_func", "test");
  ai::regist_function(std::move(func));

  auto cats = ai::get_categories();
  EXPECT_GT(cats.count("reg_cat"), 0u);
}

TEST(RegistFunction, DisabledFunctionNotRegistered) {
  auto initial = ai::get_categories();
  auto func =
      std::make_unique<TestFunction>("disabled_cat", "df", "test", "", false);
  ai::regist_function(std::move(func));

  EXPECT_EQ(ai::get_categories().count("disabled_cat"),
            initial.count("disabled_cat"));
}

// =============================================================================
// get_categories
// =============================================================================

TEST(GetCategories, ContainsBuiltinDefaultCategory) {
  auto cats = ai::get_categories();
  EXPECT_GT(cats.count("default"), 0u);
}

TEST(GetCategories, IncludesNewlyRegisteredCategory) {
  auto func = std::make_unique<TestFunction>("new_cat", "new_func", "test");
  ai::regist_function(std::move(func));

  auto cats = ai::get_categories();
  EXPECT_GT(cats.count("new_cat"), 0u);
}

// =============================================================================
// get_tools
// =============================================================================

TEST(GetTools, EmptyCategoriesReturnsEmptyArray) {
  auto tools = ai::get_tools({});
  EXPECT_TRUE(tools.is_array());
  EXPECT_EQ(tools.size(), 0u);
}

TEST(GetTools, MatchesRequestedCategory) {
  auto func =
      std::make_unique<TestFunction>("tools_cat", "tools_func", "a tool");
  ai::regist_function(std::move(func));

  auto tools = ai::get_tools({"tools_cat"});
  ASSERT_TRUE(tools.is_array());

  bool found = false;
  for (auto const& t : tools) {
    if (t.value("name", "") == "tools_func") {
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found);
}

TEST(GetTools, UnmatchedCategoryReturnsEmptyArray) {
  auto tools = ai::get_tools({"nonexistent_category_xyz_123"});
  EXPECT_TRUE(tools.is_array());
  EXPECT_EQ(tools.size(), 0u);
}

TEST(GetTools, MultipleCategoriesReturnsToolsFromAll) {
  auto f1 = std::make_unique<TestFunction>("mcat_1", "mfunc_1", "first");
  auto f2 = std::make_unique<TestFunction>("mcat_2", "mfunc_2", "second");
  ai::regist_function(std::move(f1));
  ai::regist_function(std::move(f2));

  auto tools = ai::get_tools({"mcat_1", "mcat_2"});
  bool found_1 = false;
  bool found_2 = false;
  for (auto const& t : tools) {
    if (t.value("name", "") == "mfunc_1") {
      found_1 = true;
    }
    if (t.value("name", "") == "mfunc_2") {
      found_2 = true;
    }
  }
  EXPECT_TRUE(found_1);
  EXPECT_TRUE(found_2);
}

TEST(GetTools, OnlyReturnsToolsForSpecifiedCategories) {
  auto f = std::make_unique<TestFunction>("only_cat", "only_func", "only");
  ai::regist_function(std::move(f));

  // Request a different category — our tool should not appear
  auto tools = ai::get_tools({"default"});
  bool found = false;
  for (auto const& t : tools) {
    if (t.value("name", "") == "only_func") {
      found = true;
    }
  }
  EXPECT_FALSE(found);
}

// =============================================================================
// call_tool
// =============================================================================

TEST(CallTool, ReturnsResultForMatchingFunction) {
  auto func = std::make_unique<TestFunction>("ccat", "cf", "desc", "result_ok");
  ai::regist_function(std::move(func));

  EXPECT_EQ(ai::call_tool("cf", json::object()), "result_ok");
}

TEST(CallTool, PassesArgsToFunction) {
  auto func = std::make_unique<TestFunction>("ccat", "c_args_f", "desc", "ok");
  auto* raw = func.get();
  ai::regist_function(std::move(func));

  json args = {{"key", "value"}, {"num", 42}};
  ai::call_tool("c_args_f", args);

  EXPECT_EQ(raw->last_args(), args);
}

TEST(CallTool, NotFoundReturnsErrorMessage) {
  std::string result = ai::call_tool("nonexistent_func_name", json::object());
  EXPECT_NE(result.find("not found"), std::string::npos);
  EXPECT_NE(result.find("nonexistent_func_name"), std::string::npos);
}

TEST(CallTool, CallsFirstMatchWhenMultipleWithSameName) {
  // Register two functions with the same name in different categories.
  // call_tool should call the first one registered.
  auto f1 = std::make_unique<TestFunction>("cat_a", "dup_name", "first",
                                           "first_result");
  auto f2 = std::make_unique<TestFunction>("cat_b", "dup_name", "second",
                                           "second_result");
  ai::regist_function(std::move(f1));
  ai::regist_function(std::move(f2));

  EXPECT_EQ(ai::call_tool("dup_name", json::object()), "first_result");
}

// =============================================================================
// print_toolcall_log  (global scope function)
// =============================================================================

TEST(PrintToolcallLog, DoesNotCrashWithEmptyArgs) {
  print_toolcall_log("test_func", {});
  SUCCEED();
}

TEST(PrintToolcallLog, DoesNotCrashWithArgs) {
  print_toolcall_log("test_func", {{"arg1", "val1"}, {"arg2", "val2"}});
  SUCCEED();
}

TEST(PrintToolcallLog, DoesNotCrashWithLongValue) {
  std::string long_val(200, 'x');
  print_toolcall_log("test_func", {{"long_arg", long_val}});
  SUCCEED();
}
