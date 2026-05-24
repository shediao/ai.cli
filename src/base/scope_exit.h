#pragma once
#include <type_traits>
#include <utility>

namespace ai::base {
template <typename F>
class scope_exit {
 public:
  explicit scope_exit(F&& f) noexcept(std::is_nothrow_move_constructible_v<F>)
      : fn_(std::forward<F>(f)) {}

  scope_exit(scope_exit&& other) noexcept
      : fn_(std::move(other.fn_)), active_(other.active_) {
    other.active_ = false;
  }

  scope_exit(const scope_exit&) = delete;
  scope_exit& operator=(const scope_exit&) = delete;
  scope_exit& operator=(scope_exit&&) = delete;

  ~scope_exit() {
    if (active_) {
      fn_();
    }
  }
  void release() noexcept { active_ = false; }

 private:
  F fn_;
  bool active_ = true;
};

template <typename F>
auto make_scope_exit(F&& f) {
  return scope_exit<std::decay_t<F>>{std::forward<F>(f)};
}
}  // namespace ai::base
