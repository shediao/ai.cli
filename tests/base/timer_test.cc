#include <base/timer.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <stdexcept>

using namespace std::chrono_literals;

// =============================================================================
// Timer
// =============================================================================

TEST(TimerTest, FiresCallbackAfterDuration) {
  std::atomic_bool called{false};
  {
    ai::base::Timer timer([&called] { called.store(true); }, 10ms);
    EXPECT_TRUE(timer.running());
    std::this_thread::sleep_for(50ms);
    // Timer should have fired and stopped by now
    EXPECT_TRUE(called.load());
  }
}

TEST(TimerTest, StopBeforeFiring) {
  std::atomic_bool called{false};
  ai::base::Timer timer([&called] { called.store(true); }, 500ms);
  EXPECT_TRUE(timer.running());
  timer.stop();
  EXPECT_FALSE(timer.running());
  // Give it time in case of race, but it should not fire
  std::this_thread::sleep_for(100ms);
  EXPECT_FALSE(called.load());
}

TEST(TimerTest, RunningReflectsState) {
  ai::base::Timer timer;
  EXPECT_FALSE(timer.running());

  timer.start([] {}, 1s);
  EXPECT_TRUE(timer.running());

  timer.stop();
  EXPECT_FALSE(timer.running());
}

TEST(TimerTest, DestructorStopsTimer) {
  std::atomic_bool called{false};
  {
    ai::base::Timer timer([&called] { called.store(true); }, 1s);
    EXPECT_TRUE(timer.running());
  }
  // Destructor should cancel before the callback fires
  EXPECT_FALSE(called.load());
}

TEST(TimerTest, MoveConstructor) {
  std::atomic_bool called{false};
  ai::base::Timer t1([&called] { called.store(true); }, 10ms);
  EXPECT_TRUE(t1.running());

  ai::base::Timer t2(std::move(t1));
  EXPECT_FALSE(t1.running());  // moved-from should not be running
  EXPECT_TRUE(t2.running());

  std::this_thread::sleep_for(50ms);
  EXPECT_TRUE(called.load());
}

TEST(TimerTest, MoveAssignment) {
  std::atomic_bool c1{false};
  std::atomic_bool c2{false};

  ai::base::Timer t1([&c1] { c1.store(true); }, 10ms);
  ai::base::Timer t2([&c2] { c2.store(true); }, 1s);

  t2 = std::move(t1);
  // t1 moved-from, t2 should now hold t1's timer
  EXPECT_FALSE(t1.running());
  EXPECT_TRUE(t2.running());
  EXPECT_FALSE(c2.load());  // original t2 callback cancelled

  std::this_thread::sleep_for(50ms);
  EXPECT_TRUE(c1.load());
  EXPECT_FALSE(c2.load());
}

TEST(TimerTest, SelfStopFromCallback) {
  // Calling stop() from within the callback should not deadlock.
  std::atomic_bool callback_ran{false};
  ai::base::Timer timer(
      [&callback_ran, &timer] {
        callback_ran.store(true);
        timer.stop();  // self-stop
      },
      10ms);

  std::this_thread::sleep_for(50ms);
  EXPECT_TRUE(callback_ran.load());
  EXPECT_FALSE(timer.running());
}

TEST(TimerTest, RestartRunningTimer) {
  std::atomic_bool first_called{false};
  std::atomic_bool second_called{false};

  ai::base::Timer timer([&first_called] { first_called.store(true); }, 1s);
  EXPECT_TRUE(timer.running());

  // Restart with a different callback
  timer.start([&second_called] { second_called.store(true); }, 10ms);
  EXPECT_TRUE(timer.running());

  std::this_thread::sleep_for(50ms);
  EXPECT_FALSE(first_called.load());  // original callback cancelled
  EXPECT_TRUE(second_called.load());  // new callback fired
}

TEST(TimerTest, ExceptionInCallbackIsCaught) {
  // Callback that throws should not crash; exception is caught and logged.
  ai::base::Timer timer([] { throw std::runtime_error("test exception"); },
                        10ms);

  std::this_thread::sleep_for(50ms);
  // No crash = pass. Timer should no longer be running after callback returns.
  EXPECT_FALSE(timer.running());
}

TEST(TimerTest, UnknownExceptionInCallbackIsCaught) {
  // Callback that throws a non-std exception should not crash.
  ai::base::Timer timer([] { throw 42; }, 10ms);

  std::this_thread::sleep_for(50ms);
  EXPECT_FALSE(timer.running());
}

TEST(TimerTest, AfterStaticFactory) {
  std::atomic_bool called{false};
  auto timer = ai::base::Timer::after([&called] { called.store(true); }, 10ms);
  EXPECT_TRUE(timer.running());

  std::this_thread::sleep_for(50ms);
  EXPECT_TRUE(called.load());
}

TEST(TimerTest, StopTwiceIsSafe) {
  ai::base::Timer timer([] {}, 1s);
  EXPECT_TRUE(timer.running());
  timer.stop();
  EXPECT_FALSE(timer.running());
  timer.stop();  // second stop should be a no-op
  EXPECT_FALSE(timer.running());
}

TEST(TimerTest, StartStopStart) {
  std::atomic_int count{0};
  ai::base::Timer timer;

  timer.start([&count] { ++count; }, 10ms);
  std::this_thread::sleep_for(50ms);
  EXPECT_EQ(count.load(), 1);

  timer.start([&count] { ++count; }, 10ms);
  std::this_thread::sleep_for(50ms);
  EXPECT_EQ(count.load(), 2);

  timer.start([&count] { ++count; }, 10ms);
  std::this_thread::sleep_for(50ms);
  EXPECT_EQ(count.load(), 3);
}

TEST(TimerTest, CallbackCapturesByReference) {
  // Non-trivial captures work correctly (no slicing, no dangling).
  int value = 0;
  ai::base::Timer timer([&value] { value = 42; }, 10ms);

  std::this_thread::sleep_for(50ms);
  EXPECT_EQ(value, 42);
}
