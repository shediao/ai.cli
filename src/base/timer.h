#pragma once

#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

#include "base/logging.h"

namespace ai::base {

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996)
#endif
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
class Timer {
 public:
  Timer() = default;

  template <typename F, typename Duration>
  Timer(F&& f, Duration timeout) {
    start(std::forward<F>(f), timeout);
  }

  Timer(const Timer&) = delete;
  Timer& operator=(const Timer&) = delete;

  Timer(Timer&& other) noexcept : state_{other.exchange_state(nullptr)} {}
  Timer& operator=(Timer&& other) noexcept {
    if (this != &other) {
      stop();
      store_state(other.exchange_state(nullptr));
    }
    return *this;
  }

  ~Timer() { stop(); }

  bool running() const {
    auto s = load_state();
    return s && s->running_.load();
  }

  void stop() {
    auto state = exchange_state(nullptr);
    if (!state) {
      return;
    }
    if (state->worker_.get_id() == std::this_thread::get_id()) {
      state->worker_.detach();
      return;
    }

    state->cancelled_.store(true);
    state->cv_.notify_all();

    if (state->worker_.joinable()) {
      state->worker_.join();
    }

    state->running_.store(false);
  }

  template <typename F, typename Duration>
  void start(F&& f, Duration timeout) {
    stop();
    auto state = std::make_shared<State>();
    store_state(state);

    state->cancelled_.store(false);
    state->running_.store(true);

    state->worker_ =
        std::thread([state = state, f = std::forward<F>(f), timeout]() mutable {
          std::unique_lock<std::mutex> lock(state->mutex_);
          bool cancelled = state->cv_.wait_for(
              lock, timeout, [&] { return state->cancelled_.load(); });
          if (!cancelled) {
            lock.unlock();
            try {
              f();
            } catch (std::exception const& e) {
              LOG(ERROR) << e.what();
            } catch (...) {
              LOG(ERROR) << "unknown exception";
            }
          }
          state->running_.store(false);
        });
  }

  template <typename F, typename Duration>
  static Timer after(F&& f, Duration timeout) {
    return Timer(std::forward<F>(f), timeout);
  }

 private:
  struct State {
    std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic_bool cancelled_{true};
    std::atomic_bool running_{false};
    std::thread worker_;
  };

#ifdef __cpp_lib_atomic_shared_ptr
  std::atomic<std::shared_ptr<State>> state_{};

  std::shared_ptr<State> load_state() const { return state_.load(); }
  void store_state(std::shared_ptr<State> s) { state_.store(std::move(s)); }
  std::shared_ptr<State> exchange_state(std::shared_ptr<State> s) {
    return state_.exchange(std::move(s));
  }
#else
  std::shared_ptr<State> state_;

  std::shared_ptr<State> load_state() const {
    return std::atomic_load(&state_);
  }
  void store_state(std::shared_ptr<State> s) {
    std::atomic_store(&state_, std::move(s));
  }
  std::shared_ptr<State> exchange_state(std::shared_ptr<State> s) {
    return std::atomic_exchange(&state_, std::move(s));
  }
#endif
};
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

}  // namespace ai::base
