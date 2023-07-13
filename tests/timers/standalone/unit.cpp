#include <weave/threads/blocking/wait_group.hpp>

#include <weave/timers/processors/standalone.hpp>

#include <wheels/test/framework.hpp>
#include <wheels/test/util/cpu_timer.hpp>

#include <chrono>
#include <thread>

#if !defined(TWIST_FIBERS)

using namespace weave; // NOLINT

using weave::timers::StandaloneProcessor;
using weave::threads::blocking::WaitGroup;

TEST_SUITE(Standalone){
  template <typename F>
  struct Tester : public timers::ITimer {
    explicit Tester(timers::Millis ms, F&& f) : delay_(ms), fun_(std::move(f)) {
    }

    timers::Millis GetDelay() override {
      return delay_;
    }

    void Run() noexcept override {
      std::move(fun_)();
    }

    bool WasCancelled() override {
      return false;
    }

    ~Tester() override = default;

    timers::Millis delay_;
    F fun_;
  };

  SIMPLE_TEST(JustWorks) {
    timers::StandaloneProcessor proc{};
    WaitGroup wg;

    Tester tester{5ms, [&]{
      wg.Done();
    }};

    wg.Add(1);

    proc.AddTimer(&tester);

    wg.Wait();
  }

  SIMPLE_TEST(CorrectDelay) {
    timers::StandaloneProcessor proc{};
    WaitGroup wg;
    wg.Add(1);

    Tester tester{750ms, [&wg]{
      wg.Done();
    }};

    auto start = std::chrono::steady_clock::now();

    proc.AddTimer(&tester);

    wg.Wait();

    auto finish = std::chrono::steady_clock::now();

    timers::Millis elapsed = timers::ToMillis(finish - start);

    ASSERT_GE(elapsed, 750ms);
    ASSERT_LE(elapsed, 750ms + 25ms);

  }

  SIMPLE_TEST(Dtor) {
    bool flag = false;

    Tester test{10s, [&]{
      flag = true;
    }};

    {
      timers::StandaloneProcessor proc{};
      proc.AddTimer(&test);
    }

    ASSERT_TRUE(flag);
  }

  SIMPLE_TEST(TwoTimers) {
    int count = 0;
    bool flag = false;
    WaitGroup wg;

    wg.Add(1);

    Tester first{5ms, [&]{
      flag = true;
      count++;
    }};

    Tester second{10ms, [&]{
      ASSERT_TRUE(flag);
      count++;
      wg.Done();
    }};

    timers::StandaloneProcessor proc{};

    proc.AddTimer(&first);
    proc.AddTimer(&second);

    wg.Wait();

    ASSERT_TRUE(flag);
    ASSERT_EQ(count, 2);
  }

  SIMPLE_TEST(Child) {
    timers::StandaloneProcessor proc{};

    WaitGroup wg;
    wg.Add(1);

    Tester child{5ms, [&]{
      wg.Done();
    }};

    Tester parent{5ms, [&]{
      proc.AddTimer(&child);
    }};

    proc.AddTimer(&parent);

    wg.Wait();
  }

  SIMPLE_TEST(DontWasteTime) {
    timers::StandaloneProcessor proc{};
    WaitGroup wg1;
    WaitGroup wg2;

    Tester fast{1ms, []{
      std::this_thread::sleep_for(1s);
    }};

    Tester slow{500ms, [&]{
      wg1.Done();
    }};

    Tester very_slow{2s, [&]{
      wg2.Done();
    }};

    auto start = std::chrono::steady_clock::now();

    wg1.Add(1);
    wg2.Add(1);

    proc.AddTimer(&very_slow);
    proc.AddTimer(&fast);

    std::this_thread::sleep_for(100ms);
    proc.AddTimer(&slow);

    wg1.Wait();

    auto checkpoint = std::chrono::steady_clock::now();

    timers::Millis elapsed1 = timers::ToMillis(checkpoint - start);

    ASSERT_GE(elapsed1, 100ms);
    ASSERT_LE(elapsed1, 1s + 25ms);

    wg2.Wait();
  }

  SIMPLE_TEST(DontWasteCPU) {
    wheels::ProcessCPUTimer cpu_timer;

    timers::StandaloneProcessor proc{};

    WaitGroup wg;

    wg.Add(1);

    Tester test{3s, [&]{
      wg.Done();
    }};

    proc.AddTimer(&test);

    wg.Wait();

    ASSERT_LE(cpu_timer.Spent(), 25ms);
  }
}

#endif

RUN_ALL_TESTS()