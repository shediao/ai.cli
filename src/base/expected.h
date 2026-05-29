
#pragma once

#include <cassert>
#include <exception>
#include <functional>
#include <type_traits>
#include <utility>

namespace ai::base {

template <typename E>
class unexpected {
 public:
  using error_type = E;

  constexpr explicit unexpected(error_type const& e) : error_(e) {}
  constexpr explicit unexpected(error_type&& e) : error_(std::move(e)) {}

  constexpr error_type& error() & noexcept { return error_; }
  constexpr error_type const& error() const& noexcept { return error_; }
  constexpr error_type&& error() && noexcept { return std::move(error_); }

 private:
  E error_;
};

template <typename E>
constexpr unexpected<std::decay_t<E>> make_unexpected(E&& e) {
  return unexpected<std::decay_t<E>>(std::forward<E>(e));
}

template <typename E>
class bad_expected_access : public std::exception {
 public:
  explicit bad_expected_access(E e) : error_(std::move(e)) {}
  const char* what() const noexcept override { return "bad expected access"; }
  E const& error() const& noexcept { return error_; }

 private:
  E error_;
};

template <typename T, typename E>
union expected_union {
  constexpr expected_union() {}
  constexpr ~expected_union() {}
  T value;
  E error;
};

template <typename T, typename E,
          bool Trivial = std::is_trivially_destructible<T>::value &&
                         std::is_trivially_destructible<E>::value>
struct expected_storage;

template <typename T, typename E>
struct expected_storage<T, E, true> {
  constexpr expected_storage() : has_value(false) {}
  expected_union<T, E> storage;
  bool has_value;
};

template <typename T, typename E>
struct expected_storage<T, E, false> {
  constexpr expected_storage() : has_value(false) {}
  ~expected_storage() = default;
  void distroy() {
    if (has_value) {
      storage.value.~T();
    } else {
      storage.error.~E();
    }
  }
  expected_union<T, E> storage;
  bool has_value;
};

template <typename T, typename E>
class expected : private expected_storage<T, E> {
  using base = expected_storage<T, E>;

 public:
  using value_type = T;
  using error_type = E;

  // Constructors
  constexpr expected(T const& value) { construct_value(value); }
  constexpr expected(T&& value) { construct_value(std::move(value)); }
  constexpr expected(unexpected<E> const& e) { construct_error(e.error()); }
  constexpr expected(unexpected<E>&& e) {
    construct_error(std::move(e).error());
  }
  constexpr expected(expected const& other) { copy_from(other); }
  constexpr expected(expected&& other) noexcept(
      std::is_nothrow_move_constructible_v<T> &&
      std::is_nothrow_move_constructible_v<E>) {
    move_from(std::move(other));
  }

  // Assignment
  constexpr expected& operator=(expected const& other) {
    if (this == &other) {
      return *this;
    }
    reset();
    copy_from(other);
    return *this;
  }

  constexpr expected& operator=(expected&& other) noexcept(
      std::is_nothrow_move_assignable_v<T> &&
      std::is_nothrow_move_assignable_v<E>) {
    if (this == &other) {
      return *this;
    }
    reset();
    move_from(std::move(other));
    return *this;
  }

  // Observer
  constexpr bool has_value() const noexcept { return this->base::has_value; }

  constexpr explicit operator bool() const noexcept { return has_value(); }

  constexpr T& value() & { return check_value(), this->storage.value; }
  constexpr T const& value() const& {
    return check_value(), this->storage.value;
  }
  constexpr T&& value() && {
    return check_value(), std::move(this->storage.value);
  }

  constexpr E& error() & {
    assert(!has_value());
    return this->storage.error;
  }
  constexpr E const& error() const& {
    assert(!has_value());
    return this->storage.error;
  }
  constexpr E&& error() && {
    assert(!has_value());
    return std::move(this->storage.error);
  }

  // Operators
  constexpr T& operator*() & { return value(); }
  constexpr T const& operator*() const& { return value(); }
  constexpr T&& operator*() && { return std::move(value()); }
  constexpr T* operator->() { return &value(); }
  constexpr T const* operator->() const { return &value(); }

  // Modifiers
  template <typename... Args>
  constexpr T& emplace(Args&&... args) {
    reset();
    new (&this->storage.value) T(std::forward<Args>(args)...);
    this->base::has_value = true;
    return this->storage.value;
  }

  // value_or
  template <typename U>
  constexpr T value_or(U&& default_value) const& {
    if (has_value()) {
      return this->storage.value;
    }
    return static_cast<T>(std::forward<U>(default_value));
  }

  // monadic api
  template <typename F>
  constexpr auto and_then(F&& f) {
    using result_type = std::invoke_result_t<F, T&>;
    static_assert(!std::is_void_v<result_type>,
                  "and_then must return expected");
    if (has_value()) {
      return std::invoke(std::forward<F>(f), this->storage.value);
    }
    return result_type(make_unexpected(error()));
  }

  template <typename F>
  constexpr auto transform(F&& f) -> expected<std::invoke_result_t<F, T&>, E> {
    using result_type = expected<std::invoke_result_t<F, T&>, E>;
    if (has_value()) {
      return result_type(std::invoke(std::forward<F>(f), this->storage.value));
    }
    return result_type(make_unexpected(error()));
  }

  template <typename F>
  constexpr expected or_else(F&& f) {
    if (has_value()) {
      return *this;
    }
    return std::invoke(std::forward<F>(f), error());
  }

  // swap
  constexpr void swap(expected& other) {
    using std::swap;
    if (has_value() && other.has_value()) {
      swap(this->storage.value, other.storage.value);
      return;
    }
    if (!has_value() && !other.has_value()) {
      swap(this->storage.error, other.storage.error);
      return;
    }
    expected tmp(std::move(other));
    other = std::move(*this);
    *this = std::move(tmp);
  }

  // Destructor
  constexpr ~expected() { reset(); }

 private:
  template <typename U>
  constexpr void construct_value(U&& v) {
    new (&this->storage.value) T(std::forward<U>(v));
    this->base::has_value = true;
  }

  template <typename G>
  constexpr void construct_error(G&& g) {
    new (&this->storage.error) E(std::forward<G>(g));
    this->base::has_value = false;
  }

  constexpr void reset() {
    if constexpr (!std::is_trivially_destructible<T>::value ||
                  !std::is_trivially_destructible<E>::value) {
      this->base::distroy();
    }
  }

  constexpr void check_value() const {
#ifdef __cpp_exceptions
    if (!has_value()) {
      throw bad_expected_access<E>(this->storage.error);
    }
#else
    assert(has_value());
#endif
  }

  constexpr void copy_from(const expected& other) {
    if (other.has_value()) {
      construct_value(other.storage.value);
    } else {
      construct_error(other.storage.error);
    }
  }

  constexpr void move_from(expected&& other) {
    if (other.has_value()) {
      construct_value(std::move(other.storage.value));
    } else {
      construct_error(std::move(other.storage.error));
    }
  }
};

template <typename E>
class expected<void, E> {
 public:
  using error_type = E;

  constexpr expected() noexcept : has_value_(true) {}

  constexpr expected(unexpected<E> const& e) : has_value_(false) {
    new (&error_) E(e.error());
  }

  constexpr expected(unexpected<E>&& e) : has_value_(false) {
    new (&error_) E(std::move(e.error()));
  }

  constexpr expected(expected const& other) : has_value_(other.has_value_) {
    if (!has_value_) {
      new (&error_) E(other.error_);
    }
  }

  constexpr expected(expected&& other) : has_value_(other.has_value_) {
    if (!has_value_) {
      new (&error_) E(std::move(other.error_));
    }
  }

  constexpr ~expected() {
    if (!has_value_) {
      error_.~E();
    }
  }

  constexpr bool has_value() const noexcept { return has_value_; }

  constexpr explicit operator bool() const noexcept { return has_value_; }

  constexpr void value() const { check_value(); }

  constexpr E& error() & { return error_; }

  constexpr E const& error() const& { return error_; }

  constexpr E&& error() && { return std::move(error_); }

  // Assignment
  constexpr expected& operator=(expected const& other) {
    if (this == &other) {
      return *this;
    }
    if (!has_value_) {
      error_.~E();
    }
    has_value_ = other.has_value_;
    if (!has_value_) {
      new (&error_) E(other.error_);
    }
    return *this;
  }

  constexpr expected& operator=(expected&& other) {
    if (this == &other) {
      return *this;
    }
    if (!has_value_) {
      error_.~E();
    }
    has_value_ = other.has_value_;
    if (!has_value_) {
      new (&error_) E(std::move(other.error_));
    }
    return *this;
  }

  template <typename F>
  constexpr auto and_then(F&& f) {
    using result_type = std::invoke_result_t<F>;

    if (has_value_) {
      return std::invoke(std::forward<F>(f));
    }

    return result_type(make_unexpected(error_));
  }

  // swap
  constexpr void swap(expected& other) {
    using std::swap;
    if (has_value_ && other.has_value_) {
      return;  // both void, nothing to swap
    }
    if (!has_value_ && !other.has_value_) {
      swap(error_, other.error_);
      return;
    }
    expected tmp(std::move(other));
    other = std::move(*this);
    *this = std::move(tmp);
  }

 private:
  constexpr void check_value() const {
#ifdef __cpp_exceptions
    if (!has_value_) {
      throw bad_expected_access<E>(error_);
    }
#else
    assert(has_value_);
#endif
  }

  union {
    E error_;
  };

  bool has_value_;
};

template <typename T, typename E>
constexpr bool operator==(expected<T, E> const& lhs,
                          expected<T, E> const& rhs) {
  if (lhs.has_value() != rhs.has_value()) {
    return false;
  }

  if (lhs.has_value()) {
    return lhs.value() == rhs.value();
  }

  return lhs.error() == rhs.error();
}

template <typename E>
constexpr bool operator==(expected<void, E> const& lhs,
                          expected<void, E> const& rhs) {
  if (lhs.has_value() != rhs.has_value()) {
    return false;
  }

  if (lhs.has_value()) {
    return true;  // both void, both have value
  }

  return lhs.error() == rhs.error();
}

}  // namespace ai::base
