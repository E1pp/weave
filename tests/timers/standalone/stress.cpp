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

void ManyTimersTest() {
  timers::StandaloneProcessor proc{};

  for (twist::test::Repeat repeat; repeat(); ) {
    WaitGroup wg;

    int timers = twist::test::Random(1, 10);

    wg.Add(timers);

    Tester test{1ms, [&]{
      wg.Done();
    }};

    std::vector<decltype(test)> vec{};

    for(int i = 0; i < timers; i++){
      vec.push_back(test);
    }

    for(auto& i : vec){
      proc.AddTimer(&i);
    }

    wg.Wait();
  }
}

void SpawnerTest() {
  timers::StandaloneProcessor proc{};

  for (twist::test::Repeat repeat; repeat(); ) {
    WaitGroup wg;

    wg.Add(2);
    int done = 0;

    Tester child1{1ms, [&]{

      wg.Done();
    }};

    Tester child2{1ms, [&]{

      wg.Done();
    }};

    Tester parent1{1ms, [&]{
      proc.AddTimer(&child1);
      done++;
    }};

    Tester parent2{1ms, [&]{
      proc.AddTimer(&child2);
      done++;
    }};

    proc.AddTimer(&parent1);
    proc.AddTimer(&parent2);

    wg.Wait();

    ASSERT_EQ(done, 2);
  }    
}

} // namespace tests

TEST_SUITE(Standalone) {
  TWIST_TEST_REPEAT(OneTimer, 5s) {
    tests::OneTimerTest();
  }

  TWIST_TEST_REPEAT(ManyTimers, 5s) {
    tests::ManyTimersTest();
  }

  TWIST_TEST_REPEAT(Spawner, 5s) {
    tests::SpawnerTest();
  }
}

RUN_ALL_TESTS()
