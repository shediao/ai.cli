#pragma once

#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

#include "base/logging.h"

namespace ai::base {

class Timer {
 public:
  Timer() = default;

  template <typename F, typename Duration>
  Timer(F&& f, Duration timeout) {
    start(std::forward<F>(f), timeout);
  }

  Timer(const Timer&) = delete;
  Timer& operator=(const Timer&) = delete;

  Timer(Timer&& other) noexcept
      : state_{std::atomic_exchange(&other.state_, std::shared_ptr<State>{})} {}
  Timer& operator=(Timer&& other) noexcept {
    if (this != &other) {
      stop();
      std::atomic_store(&state_, std::atomic_exchange(
                                     &other.state_, std::shared_ptr<State>{}));
    }
    return *this;
  }

  ~Timer() { stop(); }

  bool running() const {
    auto s = std::atomic_load(&state_);
    return s && s->running_.load();
  }

  void stop() {
    auto state = std::atomic_exchange(&state_, std::shared_ptr<State>{});
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
    std::atomic_store(&state_, state);

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
  std::shared_ptr<State> state_;
};

}  // namespace ai::base
