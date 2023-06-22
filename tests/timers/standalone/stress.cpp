#include <weave/threads/lockfull/wait_group.hpp>

#include <weave/timers/processors/standalone.hpp>

#include <twist/test/with/wheels/stress.hpp>

#include <twist/test/race.hpp>
#include <twist/test/budget.hpp>
#include <twist/test/repeat.hpp>
#include <twist/test/yield.hpp>
#include <twist/test/random.hpp>

using namespace weave; // NOLINT

using weave::timers::StandaloneProcessor;
using weave::threads::lockfull::WaitGroup;

namespace tests {

template <typename F>
struct Tester : public timers::ITimer {
  explicit Tester(timers::Millis ms, F&& f) : delay_(ms), fun_(std::move(f)) {
  }

  timers::Millis GetDelay() override {
    return delay_;
  }

  void Callback() override {
    std::move(fun_)();
  }

  ~Tester() override = default;

  timers::Millis delay_;
  F fun_;
};

void OneTimerTest() {
  timers::StandaloneProcessor proc{};

  for (twist::test::Repeat repeat; repeat(); ) {
    WaitGroup wg;
    wg.Add(1);

    Tester test{1ms, [&]{
      wg.Done();
    }};

    proc.AddTimer(&test);

    wg.Wait();
  }
}

void SpawnerTest() {
  timers::StandaloneProcessor proc{};

  for (twist::test::Repeat repeat; repeat(); ) {
    WaitGroup wg;
    wg.Add(1);
    bool flag = false;

    Tester child{1ms, [&]{
      ASSERT_TRUE(flag);
      wg.Done();
    }};

    Tester parent{1ms, [&]{
      proc.AddTimer(&child);
      flag = true;
    }};

    proc.AddTimer(&parent);

    wg.Wait();
  }    
}

} // namespace tests

TEST_SUITE(Standalone) {
  TWIST_TEST_REPEAT(OneTimer, 5s) {
    tests::OneTimerTest();
  }

  TWIST_TEST_REPEAT(Spawner, 5s) {
    tests::SpawnerTest();
  }
}

RUN_ALL_TESTS()



