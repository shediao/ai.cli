
#include <base/scope_exit.h>
#include <gtest/gtest.h>

// =============================================================================
// scope_exit
// =============================================================================

TEST(AutoRunTest, CallsFunctionOnDestruction) {
  int call_count = 0;
  {
    auto cleanup = [&call_count]() { ++call_count; };
    ai::base::scope_exit autorun(std::move(cleanup));
    EXPECT_EQ(call_count, 0);
  }
  EXPECT_EQ(call_count, 1);
}

TEST(AutoRunTest, CallsFunctionOnce) {
  int call_count = 0;
  {
    ai::base::scope_exit autorun([&call_count]() { ++call_count; });
    EXPECT_EQ(call_count, 0);
  }
  EXPECT_EQ(call_count, 1);
}

TEST(AutoRunTest, MoveOfLambdaPreservesBehavior) {
  int call_count = 0;
  auto lambda = [&call_count]() { ++call_count; };
  {
    ai::base::scope_exit autorun(std::move(lambda));
    EXPECT_EQ(call_count, 0);
  }
  EXPECT_EQ(call_count, 1);
}

TEST(AutoRunTest, NoArgCallableAcceptsNoCaptureLambda) {
  bool called = false;
  {
    // No-capture lambda is convertible to function pointer
    ai::base::scope_exit autorun([&called]() { called = true; });
  }
  EXPECT_TRUE(called);
}

TEST(AutoRunTest, MultipleAutoRunsCallInReverseOrder) {
  std::string order;
  {
    ai::base::scope_exit a1([&order]() { order += "1"; });
    ai::base::scope_exit a2([&order]() { order += "2"; });
    ai::base::scope_exit a3([&order]() { order += "3"; });
    // Destructors run in reverse order of construction
  }
  EXPECT_EQ(order, "321");
}
