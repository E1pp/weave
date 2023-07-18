#include <weave/executors/thread_pool.hpp>

#include <weave/fibers/sched/yield.hpp>
#include <weave/fibers/sched/sleep_for.hpp>

#include <weave/futures/make/submit.hpp>

#include <weave/futures/run/await.hpp>

#include <weave/futures/combine/par/all.hpp>
#include <weave/futures/combine/par/first.hpp>

#include <weave/futures/combine/seq/and_then.hpp>
#include <weave/futures/combine/seq/start.hpp>
#include <weave/futures/combine/seq/box.hpp>

#include <weave/futures/run/detach.hpp>

#include <weave/timers/processors/standalone.hpp>

#include <wheels/test/framework.hpp>

#include <twist/test/with/wheels/stress.hpp>

#include <twist/test/budget.hpp>

#include <atomic>
#include <chrono>

using namespace weave; // NOLINT
using namespace std::chrono_literals;

using weave::Unit;

//////////////////////////////////////////////////////////////////////

void StressTestPipeline() {
  executors::ThreadPool pool{4};
  pool.Start();

  size_t iter = 0;

  while (twist::test::KeepRunning()) {
    ++iter;

    size_t pipelines = 1 + iter % 3;

    std::atomic<size_t> counter1{0};
    std::atomic<size_t> counter2{0};
    std::atomic<size_t> counter3{0};
    std::atomic<bool> flag = false;

    for (size_t j = 0; j < pipelines; ++j) {
      auto f = futures::Submit(pool,
                               [&]() {
                                 ++counter1;
                               })
                   | futures::AndThen([&](Unit) { ++counter2;} )
                   | futures::Start()
                   | futures::Via(pool)
                   | futures::AndThen([&](Unit) { ++counter3; })
                   | futures::Start();

      futures::Submit(pool, [f = std::move(f), &flag]() mutable {
        std::move(f).RequestCancel();
        flag.store(true);
      }) | futures::Detach();
    }

    pool.WaitIdle();
    ASSERT_TRUE(flag.load());

    ASSERT_TRUE(counter1.load() >= counter2.load());
    ASSERT_TRUE(counter2.load() >= counter3.load());
  }

  pool.Stop();
}

//////////////////////////////////////////////////////////////////////

void StressTestDdosStart() {
  executors::ThreadPool pool{4};
  pool.Start();

  size_t iter = 0;

  while (twist::test::KeepRunning()) {
    ++iter;

    size_t pipelines = 1 + iter % 3;

    size_t num_attacks = 1 + iter % 15;

    std::atomic<size_t> counter1{0};
    std::atomic<size_t> counter2{0};

    for (size_t j = 0; j < pipelines; ++j) {
      auto f = futures::Submit(pool, [&]{
        ++counter1;

      }) | futures::Start() | futures::AndThen([&, num_attacks]{
        ++counter2;

        for(size_t i = 0; i < num_attacks; ++i){
          try {
            futures::Submit(pool, []{}) | futures::Start() | futures::Await();
          } catch(cancel::CancelledException){
            ASSERT_EQ(iter % 7, 0);
          }
        }
      }) | futures::Start();

      futures::Submit(pool, [f = std::move(f), iter]() mutable {
        if(iter % 7 == 0){
          std::move(f).RequestCancel();
        } else {
          std::move(f) | futures::Await();
        }  
      }) | futures::Detach();

    }

    pool.WaitIdle();

    ASSERT_TRUE(counter1.load() >= counter2.load());
  }

  pool.Stop();
}

//////////////////////////////////////////////////////////////////////

void StressTestSched() {
  executors::ThreadPool pool{4};
  pool.Start();

  timers::StandaloneProcessor proc{};
  proc.MakeGlobal();

  size_t iter = 0;

  while (twist::test::KeepRunning()) {
    ++iter;

    size_t pipelines = 1 + iter % 3;

    size_t yields = 1 + iter % 10;

    size_t timeouts = 1 + iter % 5;

    std::atomic<size_t> counter1{0};
    std::atomic<size_t> counter2{0};

    for (size_t j = 0; j < pipelines; ++j) {
      auto f = futures::Submit(pool,[&, yields] {
                                 for(size_t i = 0; i < yields; ++i){
                                  fibers::Yield();
                                 }

                                 ++counter1;
                               })
                   | futures::AndThen([&, timeouts] { 
                    for(size_t i = 0; i < timeouts; ++i){
                      fibers::SleepFor(2ms);
                    }
                    
                    ++counter2;} )
                   | futures::Start();

      futures::Submit(pool, [f = std::move(f)]() mutable {
        std::move(f).RequestCancel();
      }) | futures::Detach();
    }

    pool.WaitIdle();

    ASSERT_TRUE(counter1.load() >= counter2.load());
  }

  pool.Stop();
}

//////////////////////////////////////////////////////////////////////

TEST_SUITE(Cancellation) {
  // TWIST_TEST(StressPipeline, 5s) {
  //   StressTestPipeline();
  // }

  // TWIST_TEST(DdosStart, 5s){
  //   StressTestDdosStart();
  // }

  TWIST_TEST(StressSched, 5s){
    StressTestSched();
  }
}

RUN_ALL_TESTS()