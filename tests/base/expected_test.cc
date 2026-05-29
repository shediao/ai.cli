#include <base/expected.h>
#include <gtest/gtest.h>

#include <string>
#include <utility>

using ai::base::bad_expected_access;
using ai::base::expected;
using ai::base::make_unexpected;

// =============================================================================
// ai::base::unexpected<E>
// =============================================================================

TEST(UnexpectedTest, ConstructFromLvalue) {
  std::string err = "error";
  ai::base::unexpected<std::string> u(err);
  EXPECT_EQ(u.error(), "error");
  // Original should be unchanged (copied)
  EXPECT_EQ(err, "error");
}

TEST(UnexpectedTest, ConstructFromRvalue) {
  std::string err = "error";
  ai::base::unexpected<std::string> u(std::move(err));
  EXPECT_EQ(u.error(), "error");
}

TEST(UnexpectedTest, ErrorLvalueAccessor) {
  ai::base::unexpected<int> u(42);
  int& e = u.error();
  EXPECT_EQ(e, 42);
  e = 100;
  EXPECT_EQ(u.error(), 100);
}

TEST(UnexpectedTest, ErrorConstLvalueAccessor) {
  const ai::base::unexpected<int> u(42);
  EXPECT_EQ(u.error(), 42);
}

TEST(UnexpectedTest, ErrorRvalueAccessor) {
  ai::base::unexpected<std::string> u("hello");
  std::string e = std::move(u).error();
  EXPECT_EQ(e, "hello");
}

// =============================================================================
// make_unexpected
// =============================================================================

TEST(MakeUnexpectedTest, Lvalue) {
  std::string err = "fail";
  auto u = make_unexpected(err);
  static_assert(std::is_same_v<decltype(u), ai::base::unexpected<std::string>>);
  EXPECT_EQ(u.error(), "fail");
}

TEST(MakeUnexpectedTest, Rvalue) {
  auto u = make_unexpected(std::string("fail"));
  static_assert(std::is_same_v<decltype(u), ai::base::unexpected<std::string>>);
  EXPECT_EQ(u.error(), "fail");
}

TEST(MakeUnexpectedTest, IntLiteral) {
  auto u = make_unexpected(404);
  static_assert(std::is_same_v<decltype(u), ai::base::unexpected<int>>);
  EXPECT_EQ(u.error(), 404);
}

// =============================================================================
// bad_expected_access<E>
// =============================================================================

TEST(BadExpectedAccessTest, What) {
  bad_expected_access<int> ex(42);
  EXPECT_STREQ(ex.what(), "bad expected access");
}

TEST(BadExpectedAccessTest, Error) {
  bad_expected_access<std::string> ex("boom");
  EXPECT_EQ(ex.error(), "boom");
}

// =============================================================================
// expected<T, E> — Construction
// =============================================================================

TEST(ExpectedValueTest, ConstructFromLvalue) {
  std::string val = "hello";
  expected<std::string, int> e(val);
  EXPECT_TRUE(e.has_value());
  EXPECT_EQ(e.value(), "hello");
}

TEST(ExpectedValueTest, ConstructFromRvalue) {
  expected<std::string, int> e(std::string("hello"));
  EXPECT_TRUE(e.has_value());
  EXPECT_EQ(e.value(), "hello");
}

TEST(ExpectedValueTest, ConstructFromUnexpectedLvalue) {
  ai::base::unexpected<int> u(42);
  expected<std::string, int> e(u);
  EXPECT_FALSE(e.has_value());
  EXPECT_EQ(e.error(), 42);
}

TEST(ExpectedValueTest, ConstructFromUnexpectedRvalue) {
  expected<std::string, int> e(make_unexpected(42));
  EXPECT_FALSE(e.has_value());
  EXPECT_EQ(e.error(), 42);
}

TEST(ExpectedValueTest, CopyConstruction) {
  expected<int, std::string> e1(42);
  expected<int, std::string> e2(e1);
  EXPECT_EQ(e2.value(), 42);

  expected<int, std::string> e3(make_unexpected(std::string("err")));
  expected<int, std::string> e4(e3);
  EXPECT_EQ(e4.error(), "err");
}

TEST(ExpectedValueTest, MoveConstruction) {
  expected<std::string, int> e1("hello");
  expected<std::string, int> e2(std::move(e1));
  EXPECT_EQ(e2.value(), "hello");

  expected<std::string, int> e3(make_unexpected(404));
  expected<std::string, int> e4(std::move(e3));
  EXPECT_EQ(e4.error(), 404);
}

// =============================================================================
// expected<T, E> — Assignment
// =============================================================================

TEST(ExpectedValueTest, CopyAssignmentSameState) {
  expected<int, std::string> e1(10);
  expected<int, std::string> e2(20);
  e2 = e1;
  EXPECT_EQ(e2.value(), 10);
}

TEST(ExpectedValueTest, CopyAssignmentErrorToValue) {
  expected<int, std::string> e1(10);
  expected<int, std::string> e2(make_unexpected(std::string("oops")));
  e2 = e1;
  EXPECT_TRUE(e2.has_value());
  EXPECT_EQ(e2.value(), 10);
}

TEST(ExpectedValueTest, CopyAssignmentValueToError) {
  expected<int, std::string> e1(make_unexpected(std::string("oops")));
  expected<int, std::string> e2(10);
  e2 = e1;
  EXPECT_FALSE(e2.has_value());
  EXPECT_EQ(e2.error(), "oops");
}

TEST(ExpectedValueTest, CopyAssignmentSelf) {
  expected<int, std::string> e(42);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wself-assign-overloaded"
  e = e;
#pragma clang diagnostic pop
  EXPECT_EQ(e.value(), 42);
}

TEST(ExpectedValueTest, MoveAssignmentSameState) {
  expected<std::string, int> e1("hello");
  expected<std::string, int> e2("world");
  e2 = std::move(e1);
  EXPECT_EQ(e2.value(), "hello");
}

TEST(ExpectedValueTest, MoveAssignmentErrorToValue) {
  expected<std::string, int> e1("hello");
  expected<std::string, int> e2(make_unexpected(404));
  e2 = std::move(e1);
  EXPECT_TRUE(e2.has_value());
  EXPECT_EQ(e2.value(), "hello");
}

TEST(ExpectedValueTest, MoveAssignmentValueToError) {
  expected<std::string, int> e1(make_unexpected(404));
  expected<std::string, int> e2("hello");
  e2 = std::move(e1);
  EXPECT_FALSE(e2.has_value());
  EXPECT_EQ(e2.error(), 404);
}

TEST(ExpectedValueTest, MoveAssignmentSelf) {
  expected<int, std::string> e(42);
  // Self-move-assignment guard: should be a no-op
  expected<int, std::string>* ptr = &e;
  e = std::move(*ptr);
  EXPECT_TRUE(e.has_value());
}

// =============================================================================
// expected<T, E> — Observers
// =============================================================================

TEST(ExpectedValueTest, HasValueTrue) {
  expected<int, std::string> e(42);
  EXPECT_TRUE(e.has_value());
}

TEST(ExpectedValueTest, HasValueFalse) {
  expected<int, std::string> e(make_unexpected(std::string("err")));
  EXPECT_FALSE(e.has_value());
}

TEST(ExpectedValueTest, OperatorBoolTrue) {
  expected<int, std::string> e(42);
  EXPECT_TRUE(static_cast<bool>(e));
}

TEST(ExpectedValueTest, OperatorBoolFalse) {
  expected<int, std::string> e(make_unexpected(std::string("err")));
  EXPECT_FALSE(static_cast<bool>(e));
}

TEST(ExpectedValueTest, ValueLvalue) {
  expected<int, std::string> e(42);
  int& v = e.value();
  EXPECT_EQ(v, 42);
  v = 100;
  EXPECT_EQ(e.value(), 100);
}

TEST(ExpectedValueTest, ValueConstLvalue) {
  const expected<int, std::string> e(42);
  EXPECT_EQ(e.value(), 42);
}

TEST(ExpectedValueTest, ValueRvalue) {
  expected<std::string, int> e("hello");
  std::string v = std::move(e).value();
  EXPECT_EQ(v, "hello");
}

TEST(ExpectedValueTest, ValueThrowsOnError) {
  expected<int, std::string> e(make_unexpected(std::string("oops")));
  EXPECT_THROW(e.value(), bad_expected_access<std::string>);
}

TEST(ExpectedValueTest, ErrorLvalue) {
  expected<int, std::string> e(make_unexpected(std::string("oops")));
  std::string& err = e.error();
  EXPECT_EQ(err, "oops");
  err = "changed";
  EXPECT_EQ(e.error(), "changed");
}

TEST(ExpectedValueTest, ErrorConstLvalue) {
  const expected<int, std::string> e(make_unexpected(std::string("oops")));
  EXPECT_EQ(e.error(), "oops");
}

TEST(ExpectedValueTest, ErrorRvalue) {
  expected<int, std::string> e(make_unexpected(std::string("oops")));
  std::string err = std::move(e).error();
  EXPECT_EQ(err, "oops");
}

// =============================================================================
// expected<T, E> — Operators
// =============================================================================

TEST(ExpectedValueTest, OperatorDerefLvalue) {
  expected<int, std::string> e(42);
  int& v = *e;
  EXPECT_EQ(v, 42);
  v = 100;
  EXPECT_EQ(*e, 100);
}

TEST(ExpectedValueTest, OperatorDerefConstLvalue) {
  const expected<int, std::string> e(42);
  EXPECT_EQ(*e, 42);
}

TEST(ExpectedValueTest, OperatorDerefRvalue) {
  expected<std::string, int> e("hello");
  std::string v = *std::move(e);
  EXPECT_EQ(v, "hello");
}

TEST(ExpectedValueTest, OperatorArrow) {
  expected<std::string, int> e("hello");
  EXPECT_EQ(e->size(), 5);
}

TEST(ExpectedValueTest, OperatorArrowConst) {
  const expected<std::string, int> e("hello");
  EXPECT_EQ(e->size(), 5);
}

// =============================================================================
// expected<T, E> — Emplace
// =============================================================================

TEST(ExpectedValueTest, EmplaceOnEmpty) {
  expected<std::string, int> e(make_unexpected(404));
  e.emplace("hello world");
  EXPECT_TRUE(e.has_value());
  EXPECT_EQ(e.value(), "hello world");
}

TEST(ExpectedValueTest, EmplaceOnValue) {
  expected<std::string, int> e("old");
  e.emplace("new");
  EXPECT_EQ(e.value(), "new");
}

TEST(ExpectedValueTest, EmplaceWithArgs) {
  expected<std::string, int> e("initial");
  e.emplace(5, 'x');
  EXPECT_EQ(e.value(), "xxxxx");
}

// =============================================================================
// expected<T, E> — value_or
// =============================================================================

TEST(ExpectedValueTest, ValueOrWhenHasValue) {
  expected<int, std::string> e(42);
  EXPECT_EQ(e.value_or(0), 42);
}

TEST(ExpectedValueTest, ValueOrWhenError) {
  expected<int, std::string> e(make_unexpected(std::string("err")));
  EXPECT_EQ(e.value_or(42), 42);
}

// =============================================================================
// expected<T, E> — and_then
// =============================================================================

TEST(ExpectedValueTest, AndThenOnValue) {
  expected<int, std::string> e(5);
  auto result = e.and_then(
      [](int& v) -> expected<double, std::string> { return v * 2.0; });
  EXPECT_TRUE(result.has_value());
  EXPECT_DOUBLE_EQ(result.value(), 10.0);
}

TEST(ExpectedValueTest, AndThenOnError) {
  expected<int, std::string> e(make_unexpected(std::string("fail")));
  auto result = e.and_then(
      [](int& v) -> expected<double, std::string> { return v * 2.0; });
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "fail");
}

TEST(ExpectedValueTest, AndThenChaining) {
  expected<int, std::string> e(3);
  auto result =
      e.and_then([](int& v) -> expected<int, std::string> {
         return v * 2;
       }).and_then([](int& v) -> expected<int, std::string> { return v + 1; });
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), 7);
}

// =============================================================================
// expected<T, E> — transform
// =============================================================================

TEST(ExpectedValueTest, TransformOnValue) {
  expected<int, std::string> e(5);
  auto result = e.transform([](int& v) { return v * 2.0; });
  static_assert(
      std::is_same_v<decltype(result), expected<double, std::string>>);
  EXPECT_TRUE(result.has_value());
  EXPECT_DOUBLE_EQ(result.value(), 10.0);
}

TEST(ExpectedValueTest, TransformOnError) {
  expected<int, std::string> e(make_unexpected(std::string("fail")));
  auto result = e.transform([](int& v) { return v * 2.0; });
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "fail");
}

// =============================================================================
// expected<T, E> — or_else
// =============================================================================

TEST(ExpectedValueTest, OrElseOnValue) {
  expected<int, std::string> e(42);
  auto result = e.or_else([](std::string& err) -> expected<int, std::string> {
    return static_cast<int>(err.size());
  });
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), 42);
}

TEST(ExpectedValueTest, OrElseOnError) {
  expected<int, std::string> e(make_unexpected(std::string("fail")));
  auto result = e.or_else(
      [](std::string& /*err*/) -> expected<int, std::string> { return 999; });
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), 999);
}

// =============================================================================
// expected<T, E> — swap
// =============================================================================

TEST(ExpectedValueTest, SwapBothValue) {
  expected<int, std::string> e1(10);
  expected<int, std::string> e2(20);
  e1.swap(e2);
  EXPECT_EQ(e1.value(), 20);
  EXPECT_EQ(e2.value(), 10);
}

TEST(ExpectedValueTest, SwapBothError) {
  expected<int, std::string> e1(make_unexpected(std::string("first")));
  expected<int, std::string> e2(make_unexpected(std::string("second")));
  e1.swap(e2);
  EXPECT_EQ(e1.error(), "second");
  EXPECT_EQ(e2.error(), "first");
}

TEST(ExpectedValueTest, SwapMixed) {
  expected<int, std::string> e1(42);
  expected<int, std::string> e2(make_unexpected(std::string("oops")));
  e1.swap(e2);
  EXPECT_FALSE(e1.has_value());
  EXPECT_EQ(e1.error(), "oops");
  EXPECT_TRUE(e2.has_value());
  EXPECT_EQ(e2.value(), 42);

  // Swap back
  e1.swap(e2);
  EXPECT_TRUE(e1.has_value());
  EXPECT_EQ(e1.value(), 42);
  EXPECT_FALSE(e2.has_value());
  EXPECT_EQ(e2.error(), "oops");
}

// =============================================================================
// expected<T, E> — operator==
// =============================================================================

TEST(ExpectedValueTest, EqualBothValue) {
  expected<int, std::string> e1(42);
  expected<int, std::string> e2(42);
  EXPECT_TRUE(e1 == e2);
}

TEST(ExpectedValueTest, EqualBothValueDifferent) {
  expected<int, std::string> e1(42);
  expected<int, std::string> e2(100);
  EXPECT_FALSE(e1 == e2);
}

TEST(ExpectedValueTest, EqualBothError) {
  expected<int, std::string> e1(make_unexpected(std::string("oops")));
  expected<int, std::string> e2(make_unexpected(std::string("oops")));
  EXPECT_TRUE(e1 == e2);
}

TEST(ExpectedValueTest, EqualBothErrorDifferent) {
  expected<int, std::string> e1(make_unexpected(std::string("oops")));
  expected<int, std::string> e2(make_unexpected(std::string("fail")));
  EXPECT_FALSE(e1 == e2);
}

TEST(ExpectedValueTest, EqualMixed) {
  expected<int, std::string> e1(42);
  expected<int, std::string> e2(make_unexpected(std::string("oops")));
  EXPECT_FALSE(e1 == e2);
}

// =============================================================================
// expected<T, E> — Non-trivial types
// =============================================================================

namespace {

struct NonTrivial {
  int value;
  static int alive_count;

  explicit NonTrivial(int v) : value(v) { ++alive_count; }
  NonTrivial(NonTrivial const& other) : value(other.value) { ++alive_count; }
  NonTrivial(NonTrivial&& other) noexcept : value(other.value) {
    other.value = -1;
    ++alive_count;
  }
  ~NonTrivial() { --alive_count; }

  NonTrivial& operator=(NonTrivial const&) = default;
  NonTrivial& operator=(NonTrivial&&) = default;

  bool operator==(NonTrivial const& other) const {
    return value == other.value;
  }
};

int NonTrivial::alive_count = 0;

}  // namespace

TEST(ExpectedValueTest, NonTrivialDestruction) {
  EXPECT_EQ(NonTrivial::alive_count, 0);
  {
    expected<NonTrivial, int> e(NonTrivial(42));
    // Temporary from NonTrivial(42) is already destroyed;
    // only the moved-from copy in e's storage remains.
    EXPECT_EQ(NonTrivial::alive_count, 1);
    EXPECT_EQ(e.value().value, 42);
  }
  EXPECT_EQ(NonTrivial::alive_count, 0);
}

TEST(ExpectedValueTest, NonTrivialCopy) {
  NonTrivial nt(10);
  EXPECT_EQ(NonTrivial::alive_count, 1);
  {
    expected<NonTrivial, int> e(nt);
    EXPECT_EQ(NonTrivial::alive_count, 2);  // nt + copy in e
    expected<NonTrivial, int> e2(e);
    EXPECT_EQ(NonTrivial::alive_count, 3);  // nt + e + e2
  }
  EXPECT_EQ(NonTrivial::alive_count, 1);  // only nt remains
}

TEST(ExpectedValueTest, NonTrivialMove) {
  {
    expected<NonTrivial, int> e1(NonTrivial(42));
    // Temporary from NonTrivial(42) already destroyed; only e1's copy lives.
    EXPECT_EQ(NonTrivial::alive_count, 1);
    expected<NonTrivial, int> e2(std::move(e1));
    // e1 (moved-from, still alive) + e2 (newly moved-to).
    EXPECT_EQ(NonTrivial::alive_count, 2);
  }
  EXPECT_EQ(NonTrivial::alive_count, 0);
}

TEST(ExpectedValueTest, NonTrivialErrorDestruction) {
  EXPECT_EQ(NonTrivial::alive_count, 0);
  {
    expected<int, NonTrivial> e(make_unexpected(NonTrivial(404)));
    // Temporary and intermediate unexpected are destroyed;
    // only the moved-from copy in e's storage remains.
    EXPECT_EQ(NonTrivial::alive_count, 1);
    EXPECT_EQ(e.error().value, 404);
  }
  EXPECT_EQ(NonTrivial::alive_count, 0);
}

// =============================================================================
// expected<void, E> — Construction
// =============================================================================

TEST(ExpectedVoidTest, DefaultConstruction) {
  expected<void, int> e;
  EXPECT_TRUE(e.has_value());
}

TEST(ExpectedVoidTest, ConstructFromUnexpectedLvalue) {
  ai::base::unexpected<int> u(42);
  expected<void, int> e(u);
  EXPECT_FALSE(e.has_value());
  EXPECT_EQ(e.error(), 42);
}

TEST(ExpectedVoidTest, ConstructFromUnexpectedRvalue) {
  expected<void, int> e(make_unexpected(42));
  EXPECT_FALSE(e.has_value());
  EXPECT_EQ(e.error(), 42);
}

TEST(ExpectedVoidTest, CopyConstruction) {
  expected<void, std::string> e1;
  expected<void, std::string> e2(e1);
  EXPECT_TRUE(e2.has_value());

  expected<void, std::string> e3(make_unexpected(std::string("err")));
  expected<void, std::string> e4(e3);
  EXPECT_FALSE(e4.has_value());
  EXPECT_EQ(e4.error(), "err");
}

TEST(ExpectedVoidTest, MoveConstruction) {
  expected<void, std::string> e1;
  expected<void, std::string> e2(std::move(e1));
  EXPECT_TRUE(e2.has_value());

  expected<void, std::string> e3(make_unexpected(std::string("err")));
  expected<void, std::string> e4(std::move(e3));
  EXPECT_FALSE(e4.has_value());
  EXPECT_EQ(e4.error(), "err");
}

// =============================================================================
// expected<void, E> — Assignment
// =============================================================================

TEST(ExpectedVoidTest, CopyAssignmentSameState) {
  expected<void, int> e1;
  expected<void, int> e2;
  e2 = e1;
  EXPECT_TRUE(e2.has_value());
}

TEST(ExpectedVoidTest, CopyAssignmentErrorToValue) {
  expected<void, std::string> e1;
  expected<void, std::string> e2(make_unexpected(std::string("oops")));
  e2 = e1;
  EXPECT_TRUE(e2.has_value());
}

TEST(ExpectedVoidTest, CopyAssignmentValueToError) {
  expected<void, std::string> e1(make_unexpected(std::string("oops")));
  expected<void, std::string> e2;
  e2 = e1;
  EXPECT_FALSE(e2.has_value());
  EXPECT_EQ(e2.error(), "oops");
}

TEST(ExpectedVoidTest, CopyAssignmentSelf) {
  expected<void, int> e;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wself-assign-overloaded"
  e = e;
#pragma clang diagnostic pop
  EXPECT_TRUE(e.has_value());
}

TEST(ExpectedVoidTest, MoveAssignmentSameState) {
  expected<void, int> e1;
  expected<void, int> e2;
  e2 = std::move(e1);
  EXPECT_TRUE(e2.has_value());
}

TEST(ExpectedVoidTest, MoveAssignmentValueToError) {
  expected<void, std::string> e1(make_unexpected(std::string("oops")));
  expected<void, std::string> e2;
  e2 = std::move(e1);
  EXPECT_FALSE(e2.has_value());
  EXPECT_EQ(e2.error(), "oops");
}

TEST(ExpectedVoidTest, MoveAssignmentErrorToValue) {
  expected<void, std::string> e1;
  expected<void, std::string> e2(make_unexpected(std::string("oops")));
  e2 = std::move(e1);
  EXPECT_TRUE(e2.has_value());
}

TEST(ExpectedVoidTest, MoveAssignmentSelf) {
  expected<void, int> e;
  // Test self-move-assignment guard: should be a no-op
  expected<void, int>* ptr = &e;
  e = std::move(*ptr);
  EXPECT_TRUE(e.has_value());
}

// =============================================================================
// expected<void, E> — Observers
// =============================================================================

TEST(ExpectedVoidTest, HasValueTrue) {
  expected<void, int> e;
  EXPECT_TRUE(e.has_value());
}

TEST(ExpectedVoidTest, HasValueFalse) {
  expected<void, int> e(make_unexpected(404));
  EXPECT_FALSE(e.has_value());
}

TEST(ExpectedVoidTest, OperatorBoolTrue) {
  expected<void, int> e;
  EXPECT_TRUE(static_cast<bool>(e));
}

TEST(ExpectedVoidTest, OperatorBoolFalse) {
  expected<void, int> e(make_unexpected(404));
  EXPECT_FALSE(static_cast<bool>(e));
}

TEST(ExpectedVoidTest, ValueSucceeds) {
  expected<void, int> e;
  EXPECT_NO_THROW(e.value());
}

TEST(ExpectedVoidTest, ValueThrowsOnError) {
  expected<void, std::string> e(make_unexpected(std::string("oops")));
  EXPECT_THROW(e.value(), bad_expected_access<std::string>);
}

TEST(ExpectedVoidTest, ErrorLvalue) {
  expected<void, std::string> e(make_unexpected(std::string("oops")));
  std::string& err = e.error();
  EXPECT_EQ(err, "oops");
  err = "changed";
  EXPECT_EQ(e.error(), "changed");
}

TEST(ExpectedVoidTest, ErrorConstLvalue) {
  const expected<void, std::string> e(make_unexpected(std::string("oops")));
  EXPECT_EQ(e.error(), "oops");
}

TEST(ExpectedVoidTest, ErrorRvalue) {
  expected<void, std::string> e(make_unexpected(std::string("oops")));
  std::string err = std::move(e).error();
  EXPECT_EQ(err, "oops");
}

// =============================================================================
// expected<void, E> — and_then
// =============================================================================

TEST(ExpectedVoidTest, AndThenOnValue) {
  expected<void, int> e;
  auto result = e.and_then([]() -> expected<double, int> { return 3.14; });
  EXPECT_TRUE(result.has_value());
  EXPECT_DOUBLE_EQ(result.value(), 3.14);
}

TEST(ExpectedVoidTest, AndThenOnError) {
  expected<void, int> e(make_unexpected(404));
  auto result = e.and_then([]() -> expected<double, int> { return 3.14; });
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), 404);
}

// =============================================================================
// expected<void, E> — swap
// =============================================================================

TEST(ExpectedVoidTest, SwapBothValue) {
  expected<void, int> e1;
  expected<void, int> e2;
  e1.swap(e2);
  EXPECT_TRUE(e1.has_value());
  EXPECT_TRUE(e2.has_value());
}

TEST(ExpectedVoidTest, SwapBothError) {
  expected<void, std::string> e1(make_unexpected(std::string("first")));
  expected<void, std::string> e2(make_unexpected(std::string("second")));
  e1.swap(e2);
  EXPECT_EQ(e1.error(), "second");
  EXPECT_EQ(e2.error(), "first");
}

TEST(ExpectedVoidTest, SwapMixed) {
  expected<void, std::string> e1;
  expected<void, std::string> e2(make_unexpected(std::string("oops")));
  e1.swap(e2);
  EXPECT_FALSE(e1.has_value());
  EXPECT_EQ(e1.error(), "oops");
  EXPECT_TRUE(e2.has_value());

  e1.swap(e2);
  EXPECT_TRUE(e1.has_value());
  EXPECT_FALSE(e2.has_value());
  EXPECT_EQ(e2.error(), "oops");
}

// =============================================================================
// expected<void, E> — operator==
// =============================================================================

TEST(ExpectedVoidTest, EqualBothValue) {
  expected<void, int> e1;
  expected<void, int> e2;
  EXPECT_TRUE(e1 == e2);
}

TEST(ExpectedVoidTest, EqualBothError) {
  expected<void, std::string> e1(make_unexpected(std::string("oops")));
  expected<void, std::string> e2(make_unexpected(std::string("oops")));
  EXPECT_TRUE(e1 == e2);
}

TEST(ExpectedVoidTest, EqualBothErrorDifferent) {
  expected<void, std::string> e1(make_unexpected(std::string("oops")));
  expected<void, std::string> e2(make_unexpected(std::string("fail")));
  EXPECT_FALSE(e1 == e2);
}

TEST(ExpectedVoidTest, EqualMixed) {
  expected<void, int> e1;
  expected<void, int> e2(make_unexpected(404));
  EXPECT_FALSE(e1 == e2);
}
