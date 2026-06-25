
#pragma once

#include <cassert>
#include <cstdint>
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
class bad_expected_access;

template <>
class bad_expected_access<void> : public std::exception {
 protected:
  bad_expected_access() noexcept = default;
  bad_expected_access(const bad_expected_access&) noexcept = default;
  bad_expected_access(bad_expected_access&&) noexcept = default;
  bad_expected_access& operator=(const bad_expected_access&) noexcept = default;
  bad_expected_access& operator=(bad_expected_access&&) noexcept = default;
  ~bad_expected_access() override = default;

 public:
  const char* what() const noexcept override {
    return "bad access to expected";
  }
};

template <typename E>
class bad_expected_access : public bad_expected_access<void> {
 public:
  explicit bad_expected_access(E e) : error_(std::move(e)) {}
  E const& error() const& noexcept { return error_; }
  E& error() & noexcept { return error_; }
  E&& error() && noexcept { return std::move(error_); }
  E const&& error() const&& noexcept { return std::move(error_); }

 private:
  E error_;
};

template <typename T, typename E>
class expected_storage {
 protected:
  union storage_t {
    T value;
    E error;
    constexpr storage_t() {}
    constexpr ~storage_t() {}
  };
  enum class state : uint8_t { empty, value, error };
  storage_t storage_;
  state state_ = state::empty;

 protected:
  constexpr bool has_value_impl() const noexcept {
    return state_ == state::value;
  }
  constexpr bool has_error_impl() const noexcept {
    return state_ == state::error;
  }

  constexpr void destroy() noexcept {
    switch (state_) {
      case state::value:
        std::destroy_at(std::addressof(storage_.value));
        break;
      case state::error:
        std::destroy_at(std::addressof(storage_.error));
        break;
      case state::empty:
        break;
    }
    state_ = state::empty;
  }

  template <typename... Args>
  constexpr void construct_value(Args&&... args) {
    std::construct_at(std::addressof(storage_.value),
                      std::forward<Args>(args)...);
    state_ = state::value;
  }

  template <typename... Args>
  constexpr void construct_error(Args&&... args) {
    std::construct_at(std::addressof(storage_.error),
                      std::forward<Args>(args)...);
    state_ = state::error;
  }

 public:
  constexpr ~expected_storage() { destroy(); }
};

template <typename T, typename E>
class expected;

template <typename T>
struct is_expected : std::false_type {};

template <typename T, typename E>
struct is_expected<expected<T, E>> : std::true_type {};

template <typename T>
constexpr bool is_expected_v = is_expected<T>::value;

struct unexpect_t {
  explicit unexpect_t() = default;
};
inline constexpr unexpect_t unexpect{};

template <typename T, typename E>
class expected : private expected_storage<T, E> {
  using base = expected_storage<T, E>;

 public:
  using value_type = T;
  using error_type = E;
  using unexpected_type = unexpected<E>;
  template <typename U>
  using rebind = expected<U, error_type>;

  // Constructors
  constexpr expected() {
    static_assert(std::is_default_constructible_v<T>);
    base::construct_value();
  };
  template <typename... Args>
  constexpr expected(std::in_place_t, Args&&... args) {
    base::construct_value(std::forward<Args>(args)...);
  }
  template <typename... Args>
  constexpr expected(unexpect_t, Args&&... args) {
    base::construct_error(std::forward<Args>(args)...);
  }
  constexpr expected(T const& value) { construct_value(value); }
  constexpr expected(T&& value) { construct_value(std::move(value)); }
  constexpr expected(unexpected<E> const& e) { construct_error(e.error()); }
  constexpr expected(unexpected<E>&& e) {
    construct_error(std::move(e).error());
  }
  template <typename U>
    requires std::is_convertible_v<U, E>
  constexpr expected(unexpected<U> const& e) {
    construct_error(e.error());
  }
  template <typename U>
    requires std::is_convertible_v<U, E>
  constexpr expected(unexpected<U>&& e) {
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
    copy_from(other);
    return *this;
  }

  constexpr expected& operator=(expected&& other) noexcept(
      std::is_nothrow_move_assignable_v<T> &&
      std::is_nothrow_move_assignable_v<E>) {
    if (this == &other) {
      return *this;
    }
    move_from(std::move(other));
    return *this;
  }

  // Observer
  constexpr bool has_value() const noexcept {
    return this->base::has_value_impl();
  }

  constexpr explicit operator bool() const noexcept { return has_value(); }

  constexpr T& value() & { return check_value(), this->storage_.value; }
  constexpr T const& value() const& {
    return check_value(), this->storage_.value;
  }
  constexpr T&& value() && {
    return check_value(), std::move(this->storage_.value);
  }
  constexpr const T&& value() const&& {
    return check_value(), std::move(this->storage_.value);
  }

  constexpr E& error() & noexcept {
    assert(!has_value());
    return this->storage_.error;
  }
  constexpr E const& error() const& noexcept {
    assert(!has_value());
    return this->storage_.error;
  }
  constexpr E&& error() && noexcept {
    assert(!has_value());
    return std::move(this->storage_.error);
  }
  constexpr const E&& error() const&& noexcept {
    assert(!has_value());
    return std::move(this->storage_.error);
  }

  // Operators
  constexpr T& operator*() & noexcept { return value(); }
  constexpr T const& operator*() const& noexcept { return value(); }
  constexpr T&& operator*() && noexcept { return std::move(value()); }
  constexpr const T&& operator*() const&& noexcept {
    return std::move(value());
  }
  constexpr T* operator->() noexcept {
    return std::addressof(this->storage_.value);
  }
  constexpr T const* operator->() const noexcept {
    return std::addressof(this->storage_.value);
  }

  // Modifiers
  template <typename... Args>
  constexpr T& emplace(Args&&... args) {
    base::destroy();
    base::construct_value(std::forward<Args>(args)...);
    return this->storage_.value;
  }

  // value_or
  template <typename U = std::remove_cv_t<T>>
  constexpr T value_or(U&& default_value) const& {
    static_assert(std::is_move_constructible_v<T>,
                  "value_type has to be move constructible");
    static_assert(std::is_convertible_v<U, T>,
                  "argument has to be convertible to value_type");
    if (has_value()) {
      return this->storage_.value;
    }
    return static_cast<T>(std::forward<U>(default_value));
  }
  template <typename U = std::remove_cv_t<T>>
  constexpr T value_or(U&& default_value) && {
    static_assert(std::is_move_constructible_v<T>,
                  "value_type has to be move constructible");
    static_assert(std::is_convertible_v<U, T>,
                  "argument has to be convertible to value_type");
    if (has_value()) {
      return std::move(this->storage_.value);
    }
    return static_cast<T>(std::forward<U>(default_value));
  }

  // error_or
  template <typename G = E>
  constexpr E error_or(G&& default_value) const& {
    static_assert(std::is_copy_constructible_v<E>,
                  "error_type has to be copy constructible");
    static_assert(std::is_convertible_v<G, E>,
                  "argument has to be convertible to error_type");
    if (has_value()) {
      return std::forward<G>(default_value);
    }
    return error();
  }
  template <typename G = E>
  constexpr E error_or(G&& default_value) && {
    static_assert(std::is_copy_constructible_v<E>,
                  "error_type has to be copy constructible");
    static_assert(std::is_convertible_v<G, E>,
                  "argument has to be convertible to error_type");
    if (has_value()) {
      return std::forward<G>(default_value);
    }
    return std::move(error());
  }

  // monadic api
  template <typename F>
  constexpr auto and_then(F&& f) & {
    using result_type = std::remove_cvref_t<std::invoke_result_t<F, T&>>;
    static_assert(
        is_expected_v<result_type>,
        "The result of f(value()) must be a specialization of expected");
    static_assert(std::is_same_v<typename result_type::error_type, E>,
                  "The result of f(value()) must have the same error_type as "
                  "this expected");
    if (has_value()) {
      return std::invoke(std::forward<F>(f), this->storage_.value);
    }
    static_assert(std::is_same_v<typename result_type::error_type, E>, "");
    return result_type(unexpect, error());
  }

  template <typename F>
  constexpr auto and_then(F&& f) const& {
    using result_type = std::remove_cvref_t<std::invoke_result_t<F, const T&>>;
    static_assert(
        is_expected_v<result_type>,
        "The result of f(value()) must be a specialization of expected");
    static_assert(std::is_same_v<typename result_type::error_type, E>,
                  "The result of f(value()) must have the same error_type as "
                  "this expected");
    if (has_value()) {
      return std::invoke(std::forward<F>(f), this->storage_.value);
    }
    return result_type(unexpect, error());
  }

  template <typename F>
  constexpr auto and_then(F&& f) && {
    using result_type = std::remove_cvref_t<std::invoke_result_t<F, T&&>>;
    static_assert(
        is_expected_v<result_type>,
        "The result of f(value()) must be a specialization of expected");
    static_assert(std::is_same_v<typename result_type::error_type, E>,
                  "The result of f(value()) must have the same error_type as "
                  "this expected");
    if (has_value()) {
      return std::invoke(std::forward<F>(f), std::move(this->storage_.value));
    }
    return result_type(unexpect, std::move(error()));
  }

  template <typename F>
  constexpr auto and_then(F&& f) const&& {
    using result_type = std::remove_cvref_t<std::invoke_result_t<F, const T&&>>;
    static_assert(
        is_expected_v<result_type>,
        "The result of f(value()) must be a specialization of expected");
    static_assert(std::is_same_v<typename result_type::error_type, E>,
                  "The result of f(value()) must have the same error_type as "
                  "this expected");
    if (has_value()) {
      return std::invoke(std::forward<F>(f), std::move(this->storage_.value));
    }
    return result_type(unexpect, std::move(error()));
  }

  template <typename F>
  constexpr auto transform(F&& f) & {
    using R =
        std::remove_cv_t<std::invoke_result_t<F, decltype(this->operator*())>>;
    using result_type = expected<R, E>;
    if (has_value()) {
      if constexpr (std::is_void_v<R>) {
        std::invoke(std::forward<F>(f), this->storage_.value);
        return result_type{};
      } else {
        return result_type(
            std::invoke(std::forward<F>(f), this->storage_.value));
      }
    }
    return result_type(make_unexpected(error()));
  }

  template <typename F>
  constexpr auto transform(F&& f) const& {
    using R =
        std::remove_cv_t<std::invoke_result_t<F, decltype(this->operator*())>>;
    using result_type = expected<R, E>;
    if (has_value()) {
      if constexpr (std::is_void_v<R>) {
        std::invoke(std::forward<F>(f), this->storage_.value);
        return result_type{};
      } else {
        return result_type(
            std::invoke(std::forward<F>(f), this->storage_.value));
      }
    }
    return result_type(make_unexpected(error()));
  }

  template <typename F>
  constexpr auto transform(F&& f) && {
    using R = std::remove_cv_t<
        std::invoke_result_t<F, decltype(std::move(this->operator*()))>>;
    using result_type = expected<R, E>;
    if (has_value()) {
      if constexpr (std::is_void_v<R>) {
        std::invoke(std::forward<F>(f), std::move(this->storage_.value));
        return result_type{};
      } else {
        return result_type(
            std::invoke(std::forward<F>(f), std::move(this->storage_.value)));
      }
    }
    return result_type(make_unexpected(std::move(*this).error()));
  }

  template <typename F>
  constexpr auto transform(F&& f) const&& {
    using R = std::remove_cv_t<
        std::invoke_result_t<F, decltype(std::move(this->operator*()))>>;
    using result_type = expected<R, E>;
    if (has_value()) {
      if constexpr (std::is_void_v<R>) {
        std::invoke(std::forward<F>(f), std::move(this->storage_.value));
        return result_type{};
      } else {
        return result_type(
            std::invoke(std::forward<F>(f), std::move(this->storage_.value)));
      }
    }
    return result_type(make_unexpected(std::move(*this).error()));
  }

  template <typename F>
  constexpr auto transform_error(F&& f) & {
    using result_type =
        expected<T,
                 std::remove_cv_t<std::invoke_result_t<F, decltype(error())>>>;
    if (has_value()) {
      return result_type(std::in_place, value());
    }
    return result_type(unexpect, std::invoke(std::forward<F>(f), error()));
  }

  template <typename F>
  constexpr auto transform_error(F&& f) const& {
    using result_type =
        expected<T,
                 std::remove_cv_t<std::invoke_result_t<F, decltype(error())>>>;
    if (has_value()) {
      return result_type(std::in_place, value());
    }
    return result_type(unexpect, std::invoke(std::forward<F>(f), error()));
  }

  template <typename F>
  constexpr auto transform_error(F&& f) && {
    using result_type =
        expected<T, std::remove_cv_t<
                        std::invoke_result_t<F, decltype(std::move(error()))>>>;
    if (has_value()) {
      return result_type(std::in_place, std::move(value()));
    }
    return result_type(unexpect,
                       std::invoke(std::forward<F>(f), std::move(error())));
  }

  template <typename F>
  constexpr auto transform_error(F&& f) const&& {
    using result_type =
        expected<T, std::remove_cv_t<
                        std::invoke_result_t<F, decltype(std::move(error()))>>>;
    if (has_value()) {
      return result_type(std::in_place, std::move(value()));
    }
    return result_type(unexpect,
                       std::invoke(std::forward<F>(f), std::move(error())));
  }

  template <typename F>
  constexpr auto or_else(F&& f) & {
    using result_type = std::remove_cvref_t<std::invoke_result_t<F, E&>>;
    static_assert(
        is_expected_v<result_type>,
        "The result of f(error()) must be a specialization of expected");
    static_assert(std::is_same_v<typename result_type::value_type, T>,
                  "The result of f(error()) must have the same value_type as "
                  "this expected");
    if (has_value()) {
      return result_type(std::in_place, this->value());
    }
    return std::invoke(std::forward<F>(f), error());
  }

  template <typename F>
  constexpr auto or_else(F&& f) const& {
    using result_type = std::remove_cvref_t<std::invoke_result_t<F, E const&>>;
    static_assert(
        is_expected_v<result_type>,
        "The result of f(error()) must be a specialization of expected");
    static_assert(std::is_same_v<typename result_type::value_type, T>,
                  "The result of f(error()) must have the same value_type as "
                  "this expected");
    if (has_value()) {
      return result_type(std::in_place, this->value());
    }
    return std::invoke(std::forward<F>(f), error());
  }

  template <typename F>
  constexpr auto or_else(F&& f) && {
    using result_type = std::remove_cvref_t<std::invoke_result_t<F, E&&>>;
    static_assert(
        is_expected_v<result_type>,
        "The result of f(error()) must be a specialization of expected");
    static_assert(std::is_same_v<typename result_type::value_type, T>,
                  "The result of f(error()) must have the same value_type as "
                  "this expected");
    if (has_value()) {
      return result_type(std::in_place, std::move(value()));
    }
    return std::invoke(std::forward<F>(f), std::move(error()));
  }

  template <typename F>
  constexpr auto or_else(F&& f) const&& {
    using result_type = std::remove_cvref_t<std::invoke_result_t<F, E const&&>>;
    static_assert(
        is_expected_v<result_type>,
        "The result of f(error()) must be a specialization of expected");
    static_assert(std::is_same_v<typename result_type::value_type, T>,
                  "The result of f(error()) must have the same value_type as "
                  "this expected");
    if (has_value()) {
      return result_type(std::in_place, std::move(value()));
    }
    return std::invoke(std::forward<F>(f), std::move(error()));
  }

  // swap
  constexpr void swap(expected& other) noexcept(
      std::is_nothrow_move_constructible_v<T> &&
      std::is_nothrow_swappable_v<T> &&
      std::is_nothrow_move_constructible_v<E> &&
      std::is_nothrow_swappable_v<E>) {
    using std::swap;
    if (base::has_value_impl() && other.base::has_value_impl()) {
      swap(this->storage_.value, other.storage_.value);
    } else if (base::has_error_impl() && other.base::has_error_impl()) {
      swap(this->storage_.error, other.storage_.error);
    } else if (base::has_value_impl() && !other.base::has_value_impl()) {
      if constexpr (std::is_nothrow_move_constructible_v<E>) {
        E temp{std::move(other.storage_.error)};
        other.base::destroy();
        if constexpr (std::is_nothrow_move_constructible_v<T>) {
          other.base::construct_value(std::move(this->storage_.value));
          base::destroy();
          base::construct_error(std::move(temp));
        } else {
          try {
            other.base::construct_value(std::move(this->storage_.value));
            base::destroy();
            base::construct_error(std::move(temp));
          } catch (...) {
            other.base::construct_error(std::move(temp));
            throw;
          }
        }
      } else {
        T temp{std::move(this->storage_.value)};
        base::destroy();
        try {
          base::construct_error(std::move(other.storage_.error));
          other.base::destroy();
          other.base::construct_value(std::move(temp));
        } catch (...) {
          base::construct_value(std::move(temp));
          throw;
        }
      }
    } else {
      other.swap(*this);
    }
  }

  // Destructor
  constexpr ~expected() = default;

  bool operator==(const expected& other) const {
    if (base::has_value_impl() && other.base::has_value_impl()) {
      return this->storage_.value == other.storage_.value;
    }
    if (base::has_error_impl() && other.base::has_error_impl()) {
      return this->storage_.error == other.storage_.error;
    }
    return false;
  }

  bool operator==(const T& value) const {
    return has_value() && this->storage_.value == value;
  }

 private:
  template <typename U>
  constexpr void construct_value(U&& v) {
    base::construct_value(std::forward<U>(v));
  }

  template <typename G>
  constexpr void construct_error(G&& g) {
    base::construct_error(std::forward<G>(g));
  }

  constexpr void check_value() const {
#ifdef __cpp_exceptions
    if (!has_value()) {
      throw bad_expected_access<E>(this->storage_.error);
    }
#else
    assert(has_value());
#endif
  }

  constexpr void copy_from(const expected& other) {
    if (base::has_value_impl() && other.base::has_value_impl()) {
      this->storage_.value = other.storage_.value;
      return;
    }
    if (base::has_error_impl() && other.base::has_error_impl()) {
      this->storage_.error = other.storage_.error;
      return;
    }
    if (other.base::has_value_impl()) {
      base::destroy();
      construct_value(other.storage_.value);
    } else if (other.base::has_error_impl()) {
      base::destroy();
      construct_error(other.storage_.error);
    } else {
      base::destroy();
    }
  }

  constexpr void move_from(expected&& other) {
    if (base::has_value_impl() && other.base::has_value_impl()) {
      this->storage_.value = std::move(other.storage_.value);
      return;
    }
    if (base::has_error_impl() && other.base::has_error_impl()) {
      this->storage_.error = std::move(other.storage_.error);
      return;
    }
    if (other.base::has_value_impl()) {
      base::destroy();
      construct_value(std::move(other.storage_.value));
    } else if (other.base::has_error_impl()) {
      base::destroy();
      construct_error(std::move(other.storage_.error));
    } else {
      base::destroy();
    }
  }
};

template <typename T, typename E>
class expected<T&, E> : private expected_storage<T*, E> {
  using base = expected_storage<T*, E>;

 public:
  using value_type = T&;
  using error_type = E;

  // Constructors
  constexpr expected(T& value) { base::construct_value(&value); }
  template <typename... Args>
  constexpr expected(unexpect_t, Args&&... args) {
    base::construct_error(std::forward<Args>(args)...);
  }
  constexpr expected(unexpected<E> const& e) {
    base::construct_error(e.error());
  }
  constexpr expected(unexpected<E>&& e) {
    base::construct_error(std::move(e).error());
  }
  template <typename U>
    requires std::is_convertible_v<U, E>
  constexpr expected(unexpected<U> const& e) {
    base::construct_error(e.error());
  }
  template <typename U>
    requires std::is_convertible_v<U, E>
  constexpr expected(unexpected<U>&& e) {
    base::construct_error(std::move(e).error());
  }

  constexpr expected(expected const& other) {
    if (other.has_value_impl()) {
      base::construct_value(other.storage_.value);
    } else {
      base::construct_error(other.storage_.error);
    }
  }

  constexpr expected(expected&& other) {
    if (other.has_value_impl()) {
      base::construct_value(other.storage_.value);
    } else {
      base::construct_error(std::move(other.storage_.error));
    }
  }

  // Destructor — base handles cleanup
  constexpr ~expected() = default;

  // Assignment
  constexpr expected& operator=(expected const& other) {
    if (this == &other) {
      return *this;
    }
    base::destroy();
    if (other.has_value_impl()) {
      base::construct_value(other.storage_.value);
    } else {
      base::construct_error(other.storage_.error);
    }
    return *this;
  }

  constexpr expected& operator=(expected&& other) {
    if (this == &other) {
      return *this;
    }
    base::destroy();
    if (other.has_value_impl()) {
      base::construct_value(other.storage_.value);
    } else {
      base::construct_error(std::move(other.storage_.error));
    }
    return *this;
  }

  // Observer
  constexpr bool has_value() const noexcept { return base::has_value_impl(); }

  constexpr explicit operator bool() const noexcept { return has_value(); }

  constexpr T& value() & { return check_value(), *this->storage_.value; }
  constexpr T const& value() const& {
    return check_value(), *this->storage_.value;
  }
  constexpr T& value() && {
    return check_value(), std::move(*this->storage_.value);
  }
  constexpr const T& value() const&& {
    return check_value(), std::move(*this->storage_.value);
  }

  constexpr E& error() & noexcept {
    assert(!has_value());
    return this->storage_.error;
  }
  constexpr E const& error() const& noexcept {
    assert(!has_value());
    return this->storage_.error;
  }
  constexpr E&& error() && noexcept {
    assert(!has_value());
    return std::move(this->storage_.error);
  }
  constexpr const E&& error() const&& noexcept {
    assert(!has_value());
    return std::move(this->storage_.error);
  }

  // Operators
  constexpr T& operator*() & noexcept { return value(); }
  constexpr T const& operator*() const& noexcept { return value(); }
  constexpr T& operator*() && noexcept { return value(); }
  constexpr const T& operator*() const&& noexcept { return std::move(value()); }
  constexpr T* operator->() noexcept { return this->storage_.value; }
  constexpr T const* operator->() const noexcept {
    return this->storage_.value;
  }

  // Modifiers — rebind the reference
  constexpr T& emplace(T& value) noexcept {
    base::destroy();
    base::construct_value(&value);
    return *this->storage_.value;
  }

  // value_or
  template <typename U>
  constexpr T value_or(U&& default_value) const& {
    if (has_value()) {
      return *this->storage_.value;
    }
    return static_cast<T>(std::forward<U>(default_value));
  }

  template <typename U>
  constexpr T value_or(U&& default_value) && {
    if (has_value()) {
      return std::move(*this->storage_.value);
    }
    return static_cast<T>(std::forward<U>(default_value));
  }

  // error_or
  template <typename G = E>
  constexpr E error_or(G&& default_value) const& {
    static_assert(std::is_copy_constructible_v<E>,
                  "error_type has to be copy constructible");
    static_assert(std::is_convertible_v<G, E>,
                  "argument has to be convertible to error_type");
    if (has_value()) {
      return std::forward<G>(default_value);
    }
    return error();
  }
  template <typename G = E>
  constexpr E error_or(G&& default_value) && {
    static_assert(std::is_copy_constructible_v<E>,
                  "error_type has to be copy constructible");
    static_assert(std::is_convertible_v<G, E>,
                  "argument has to be convertible to error_type");
    if (has_value()) {
      return std::forward<G>(default_value);
    }
    return std::move(error());
  }

  // monadic api
  template <typename F>
  constexpr auto and_then(F&& f) & {
    using result_type = std::remove_cvref_t<std::invoke_result_t<F, T&>>;
    static_assert(
        is_expected_v<result_type>,
        "The result of f(value()) must be a specialization of expected");
    static_assert(std::is_same_v<typename result_type::error_type, E>,
                  "The result of f(value()) must have the same error_type as "
                  "this expected");
    if (has_value()) {
      return std::invoke(std::forward<F>(f), *this->storage_.value);
    }
    return result_type(make_unexpected(error()));
  }

  template <typename F>
  constexpr auto and_then(F&& f) const& {
    using result_type = std::remove_cvref_t<std::invoke_result_t<F, const T&>>;
    static_assert(
        is_expected_v<result_type>,
        "The result of f(value()) must be a specialization of expected");
    static_assert(std::is_same_v<typename result_type::error_type, E>,
                  "The result of f(value()) must have the same error_type as "
                  "this expected");
    if (has_value()) {
      return std::invoke(std::forward<F>(f), *this->storage_.value);
    }
    return result_type(make_unexpected(error()));
  }

  template <typename F>
  constexpr auto and_then(F&& f) && {
    using result_type = std::remove_cvref_t<std::invoke_result_t<F, T&>>;
    static_assert(
        is_expected_v<result_type>,
        "The result of f(value()) must be a specialization of expected");
    static_assert(std::is_same_v<typename result_type::error_type, E>,
                  "The result of f(value()) must have the same error_type as "
                  "this expected");
    if (has_value()) {
      return std::invoke(std::forward<F>(f), *this->storage_.value);
    }
    return result_type(make_unexpected(std::move(error())));
  }

  template <typename F>
  constexpr auto and_then(F&& f) const&& {
    using result_type = std::remove_cvref_t<std::invoke_result_t<F, const T&>>;
    static_assert(
        is_expected_v<result_type>,
        "The result of f(value()) must be a specialization of expected");
    static_assert(std::is_same_v<typename result_type::error_type, E>,
                  "The result of f(value()) must have the same error_type as "
                  "this expected");
    if (has_value()) {
      return std::invoke(std::forward<F>(f), *this->storage_.value);
    }
    return result_type(make_unexpected(std::move(error())));
  }

  template <typename F>
  constexpr auto transform(F&& f) & {
    using R = std::remove_cv_t<std::invoke_result_t<F, T&>>;
    using result_type = expected<R, E>;
    if (has_value()) {
      if constexpr (std::is_void_v<R>) {
        std::invoke(std::forward<F>(f), *this->storage_.value);
        return result_type{};
      } else {
        return result_type(
            std::invoke(std::forward<F>(f), *this->storage_.value));
      }
    }
    return result_type(make_unexpected(error()));
  }

  template <typename F>
  constexpr auto transform(F&& f) const& {
    using R = std::remove_cv_t<std::invoke_result_t<F, const T&>>;
    using result_type = expected<R, E>;
    if (has_value()) {
      if constexpr (std::is_void_v<R>) {
        std::invoke(std::forward<F>(f), *this->storage_.value);
        return result_type{};
      } else {
        return result_type(
            std::invoke(std::forward<F>(f), *this->storage_.value));
      }
    }
    return result_type(make_unexpected(error()));
  }

  template <typename F>
  constexpr auto transform(F&& f) && {
    using R = std::remove_cv_t<std::invoke_result_t<F, T&>>;
    using result_type = expected<R, E>;
    if (has_value()) {
      if constexpr (std::is_void_v<R>) {
        std::invoke(std::forward<F>(f), *this->storage_.value);
        return result_type{};
      } else {
        return result_type(
            std::invoke(std::forward<F>(f), *this->storage_.value));
      }
    }
    return result_type(make_unexpected(std::move(*this).error()));
  }

  template <typename F>
  constexpr auto transform(F&& f) const&& {
    using R = std::remove_cv_t<std::invoke_result_t<F, const T&>>;
    using result_type = expected<R, E>;
    if (has_value()) {
      if constexpr (std::is_void_v<R>) {
        std::invoke(std::forward<F>(f), *this->storage_.value);
        return result_type{};
      } else {
        return result_type(
            std::invoke(std::forward<F>(f), *this->storage_.value));
      }
    }
    return result_type(make_unexpected(std::move(*this).error()));
  }

  template <typename F>
  constexpr auto transform_error(F&& f) & {
    using result_type =
        expected<T&,
                 std::remove_cv_t<std::invoke_result_t<F, decltype(error())>>>;
    if (has_value()) {
      return result_type(*this->storage_.value);
    }
    return result_type(unexpect, std::invoke(std::forward<F>(f), error()));
  }

  template <typename F>
  constexpr auto transform_error(F&& f) const& {
    using result_type =
        expected<T&,
                 std::remove_cv_t<std::invoke_result_t<F, decltype(error())>>>;
    if (has_value()) {
      return result_type(*this->storage_.value);
    }
    return result_type(unexpect, std::invoke(std::forward<F>(f), error()));
  }

  template <typename F>
  constexpr auto transform_error(F&& f) && {
    using result_type = expected<T&, std::remove_cv_t<std::invoke_result_t<
                                         F, decltype(std::move(error()))>>>;
    if (has_value()) {
      return result_type(*this->storage_.value);
    }
    return result_type(unexpect,
                       std::invoke(std::forward<F>(f), std::move(error())));
  }

  template <typename F>
  constexpr auto transform_error(F&& f) const&& {
    using result_type = expected<T&, std::remove_cv_t<std::invoke_result_t<
                                         F, decltype(std::move(error()))>>>;
    if (has_value()) {
      return result_type(*this->storage_.value);
    }
    return result_type(unexpect,
                       std::invoke(std::forward<F>(f), std::move(error())));
  }

  template <typename F>
  constexpr expected or_else(F&& f) & {
    using result_type = std::remove_cvref_t<std::invoke_result_t<F, E&>>;
    static_assert(
        is_expected_v<result_type>,
        "The result of f(error()) must be a specialization of expected");
    static_assert(std::is_same_v<typename result_type::value_type, T&>,
                  "The result of f(error()) must have the same value_type as "
                  "this expected");
    if (has_value()) {
      return result_type(value());
    }
    return std::invoke(std::forward<F>(f), error());
  }

  template <typename F>
  constexpr expected or_else(F&& f) const& {
    using result_type = std::remove_cvref_t<std::invoke_result_t<F, E const&>>;
    static_assert(
        is_expected_v<result_type>,
        "The result of f(error()) must be a specialization of expected");
    static_assert(std::is_same_v<typename result_type::value_type, T&>,
                  "The result of f(error()) must have the same value_type as "
                  "this expected");
    if (has_value()) {
      return result_type(value());
    }
    return std::invoke(std::forward<F>(f), error());
  }

  template <typename F>
  constexpr expected or_else(F&& f) && {
    using result_type = std::remove_cvref_t<std::invoke_result_t<F, E&&>>;
    static_assert(
        is_expected_v<result_type>,
        "The result of f(error()) must be a specialization of expected");
    static_assert(std::is_same_v<typename result_type::value_type, T&>,
                  "The result of f(error()) must have the same value_type as "
                  "this expected");
    if (has_value()) {
      return result_type(value());
    }
    return std::invoke(std::forward<F>(f), std::move(error()));
  }

  template <typename F>
  constexpr expected or_else(F&& f) const&& {
    using result_type = std::remove_cvref_t<std::invoke_result_t<F, E const&&>>;
    static_assert(
        is_expected_v<result_type>,
        "The result of f(error()) must be a specialization of expected");
    static_assert(std::is_same_v<typename result_type::value_type, T&>,
                  "The result of f(error()) must have the same value_type as "
                  "this expected");
    if (has_value()) {
      return result_type(value());
    }
    return std::invoke(std::forward<F>(f), std::move(error()));
  }

  // swap
  constexpr void swap(expected& other) noexcept(
      std::is_nothrow_move_constructible_v<E> &&
      std::is_nothrow_swappable_v<E>) {
    using std::swap;
    if (has_value() && other.has_value()) {
      swap(this->storage_.value, other.storage_.value);
    } else if (!has_value() && !other.has_value()) {
      swap(this->storage_.error, other.storage_.error);
    } else if (has_value() && !other.has_value()) {
      if constexpr (std::is_nothrow_move_constructible_v<E>) {
        E temp{std::move(other.storage_.error)};
        other.base::destroy();
        if constexpr (std::is_nothrow_move_constructible_v<T*>) {
          other.base::construct_value(std::move(this->storage_.value));
          base::destroy();
          base::construct_error(std::move(temp));
        } else {
          try {
            other.base::construct_value(std::move(this->storage_.value));
            base::destroy();
            base::construct_error(std::move(temp));
          } catch (...) {
            other.base::construct_error(std::move(temp));
            throw;
          }
        }
      } else {
        auto* temp = this->storage_.value;
        base::destroy();
        try {
          base::construct_error(std::move(other.storage_.error));
          other.base::destroy();
          other.base::construct_value(temp);
        } catch (...) {
          base::construct_value(temp);
          throw;
        }
      }
    } else {
      other.swap(*this);
    }
  }

  bool operator==(expected const& other) const {
    if (has_value() && other.has_value()) {
      return *this->storage_.value == *other.storage_.value;
    }
    if (!has_value() && !other.has_value()) {
      return this->storage_.error == other.storage_.error;
    }
    return false;
  }

  bool operator==(T const& value) const {
    return has_value() && *this->storage_.value == value;
  }

 private:
  constexpr void check_value() const {
#ifdef __cpp_exceptions
    if (!has_value()) {
      throw bad_expected_access<E>(this->storage_.error);
    }
#else
    assert(has_value());
#endif
  }
};

template <typename E>
class expected<void, E> {
 public:
  using error_type = E;
  using value_type = void;

  constexpr expected() noexcept : has_value_(true) {}
  constexpr expected(std::in_place_t) noexcept : has_value_(true) {}

  template <typename... Args>
  constexpr expected(unexpect_t, Args&&... args) : has_value_(false) {
    new (&error_) E(std::forward<Args>(args)...);
  }

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
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
  constexpr ~expected() {
    if (!has_value_) {
      error_.~E();
    }
  }
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

  constexpr bool has_value() const noexcept { return has_value_; }

  constexpr explicit operator bool() const noexcept { return has_value_; }

  constexpr void value() const& { check_value(); }
  constexpr void value() && { check_value(); }

  constexpr E& error() & noexcept {
    assert(!has_value_);
    return error_;
  }

  constexpr E const& error() const& noexcept {
    assert(!has_value_);
    return error_;
  }

  constexpr E&& error() && noexcept {
    assert(!has_value_);
    return std::move(error_);
  }
  constexpr const E&& error() const&& noexcept {
    assert(!has_value_);
    return std::move(error_);
  }

  constexpr void operator*() const { check_value(); }

  constexpr void emplace() noexcept { has_value_ = true; }

  // error_or
  template <typename G = E>
  constexpr E error_or(G&& default_value) const& {
    if (has_value()) {
      return std::forward<G>(default_value);
    }
    return error();
  }
  template <typename G = E>
  constexpr E error_or(G&& default_value) && {
    if (has_value()) {
      return std::forward<G>(default_value);
    }
    return std::move(error());
  }

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
  constexpr auto and_then(F&& f) & {
    using result_type = std::invoke_result_t<F>;
    if (has_value_) {
      return std::invoke(std::forward<F>(f));
    }
    return result_type(make_unexpected(error_));
  }

  template <typename F>
  constexpr auto and_then(F&& f) const& {
    using result_type = std::invoke_result_t<F>;
    if (has_value_) {
      return std::invoke(std::forward<F>(f));
    }
    return result_type(make_unexpected(error_));
  }

  template <typename F>
  constexpr auto and_then(F&& f) && {
    using result_type = std::invoke_result_t<F>;
    if (has_value_) {
      return std::invoke(std::forward<F>(f));
    }
    return result_type(make_unexpected(std::move(*this).error()));
  }

  template <typename F>
  constexpr auto and_then(F&& f) const&& {
    using result_type = std::invoke_result_t<F>;
    if (has_value_) {
      return std::invoke(std::forward<F>(f));
    }
    return result_type(make_unexpected(std::move(*this).error()));
  }

  template <typename F>
  constexpr auto transform(F&& f) & {
    using R = std::remove_cv_t<std::invoke_result_t<F>>;
    using result_type = expected<R, E>;
    if (has_value_) {
      if constexpr (std::is_void_v<R>) {
        std::invoke(std::forward<F>(f));
        return result_type{};
      } else {
        return result_type(std::invoke(std::forward<F>(f)));
      }
    }
    return result_type(make_unexpected(error_));
  }

  template <typename F>
  constexpr auto transform(F&& f) const& {
    using R = std::remove_cv_t<std::invoke_result_t<F>>;
    using result_type = expected<R, E>;
    if (has_value_) {
      if constexpr (std::is_void_v<R>) {
        std::invoke(std::forward<F>(f));
        return result_type{};
      } else {
        return result_type(std::invoke(std::forward<F>(f)));
      }
    }
    return result_type(make_unexpected(error_));
  }

  template <typename F>
  constexpr auto transform(F&& f) && {
    using R = std::remove_cv_t<std::invoke_result_t<F>>;
    using result_type = expected<R, E>;
    if (has_value_) {
      if constexpr (std::is_void_v<R>) {
        std::invoke(std::forward<F>(f));
        return result_type{};
      } else {
        return result_type(std::invoke(std::forward<F>(f)));
      }
    }
    return result_type(make_unexpected(std::move(*this).error()));
  }

  template <typename F>
  constexpr auto transform(F&& f) const&& {
    using R = std::remove_cv_t<std::invoke_result_t<F>>;
    using result_type = expected<R, E>;
    if (has_value_) {
      if constexpr (std::is_void_v<R>) {
        std::invoke(std::forward<F>(f));
        return result_type{};
      } else {
        return result_type(std::invoke(std::forward<F>(f)));
      }
    }
    return result_type(make_unexpected(std::move(*this).error()));
  }

  template <typename F>
  constexpr auto transform_error(F&& f) & {
    using result_type =
        expected<void,
                 std::remove_cv_t<std::invoke_result_t<F, decltype(error())>>>;
    if (has_value()) {
      return result_type();
    }
    return result_type(unexpect, std::invoke(std::forward<F>(f), error()));
  }

  template <typename F>
  constexpr auto transform_error(F&& f) const& {
    using result_type =
        expected<void,
                 std::remove_cv_t<std::invoke_result_t<F, decltype(error())>>>;
    if (has_value()) {
      return result_type();
    }
    return result_type(unexpect, std::invoke(std::forward<F>(f), error()));
  }

  template <typename F>
  constexpr auto transform_error(F&& f) && {
    using result_type = expected<void, std::remove_cv_t<std::invoke_result_t<
                                           F, decltype(std::move(error()))>>>;
    if (has_value()) {
      return result_type();
    }
    return result_type(unexpect,
                       std::invoke(std::forward<F>(f), std::move(error())));
  }

  template <typename F>
  constexpr auto transform_error(F&& f) const&& {
    using result_type = expected<void, std::remove_cv_t<std::invoke_result_t<
                                           F, decltype(std::move(error()))>>>;
    if (has_value()) {
      return result_type();
    }
    return result_type(unexpect,
                       std::invoke(std::forward<F>(f), std::move(error())));
  }

  template <typename F>
  constexpr expected or_else(F&& f) & {
    using result_type = std::remove_cvref_t<std::invoke_result_t<F, E&>>;
    static_assert(
        is_expected_v<result_type>,
        "The result of f(error()) must be a specialization of expected");
    static_assert(std::is_same_v<typename result_type::value_type, void>,
                  "The result of f(error()) must have the same value_type as "
                  "this expected");
    if (has_value_) {
      return result_type{};
    }
    return std::invoke(std::forward<F>(f), error_);
  }

  template <typename F>
  constexpr expected or_else(F&& f) const& {
    using result_type = std::remove_cvref_t<std::invoke_result_t<F, E const&>>;
    static_assert(
        is_expected_v<result_type>,
        "The result of f(error()) must be a specialization of expected");
    static_assert(std::is_same_v<typename result_type::value_type, void>,
                  "The result of f(error()) must have the same value_type as "
                  "this expected");
    if (has_value_) {
      return result_type{};
    }
    return std::invoke(std::forward<F>(f), error_);
  }

  template <typename F>
  constexpr expected or_else(F&& f) && {
    using result_type = std::remove_cvref_t<std::invoke_result_t<F, E&&>>;
    static_assert(
        is_expected_v<result_type>,
        "The result of f(error()) must be a specialization of expected");
    static_assert(std::is_same_v<typename result_type::value_type, void>,
                  "The result of f(error()) must have the same value_type as "
                  "this expected");
    if (has_value_) {
      return result_type{};
    }
    return std::invoke(std::forward<F>(f), std::move(error()));
  }

  template <typename F>
  constexpr expected or_else(F&& f) const&& {
    using result_type = std::remove_cvref_t<std::invoke_result_t<F, E const&&>>;
    static_assert(
        is_expected_v<result_type>,
        "The result of f(error()) must be a specialization of expected");
    static_assert(std::is_same_v<typename result_type::value_type, void>,
                  "The result of f(error()) must have the same value_type as "
                  "this expected");
    if (has_value_) {
      return result_type{};
    }
    return std::invoke(std::forward<F>(f), std::move(error()));
  }

  // swap
  constexpr void swap(expected& other) noexcept(
      std::is_nothrow_move_constructible_v<E> &&
      std::is_nothrow_swappable_v<E>) {
    using std::swap;
    if (has_value() && other.has_value()) {
    } else if (!has_value() && !other.has_value()) {
      swap(this->error_, other.error_);
    } else if (has_value() && !other.has_value()) {
      if constexpr (std::is_nothrow_move_constructible_v<E>) {
        E temp{std::move(other.error_)};
        std::destroy_at(&(other.error_));
        std::construct_at(&(this->error_), std::move(temp));
      } else {
        try {
          std::construct_at(&(this->error_), std::move(other.error_));
          std::destroy_at(&(other.error_));
        } catch (...) {
          throw;
        }
      }
      this->has_value_ = false;
      other.has_value_ = true;
    } else {
      other.swap(*this);
    }
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
