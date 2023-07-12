#include <weave/executors/thread_pool.hpp>
#include <weave/executors/submit.hpp>

#include <weave/threads/lockfull/wait_group.hpp>

#include <twist/test/with/wheels/stress.hpp>

#include <twist/test/repeat.hpp>

#include <twist/ed/stdlike/thread.hpp>
#include <twist/ed/stdlike/chrono.hpp>

#include <fmt/core.h>
#include <fmt/std.h>

#include <chrono>

using namespace weave; // NOLINT
using namespace std::chrono_literals;

// Parking + Balancing is hard

#if defined(TWIST_FIBERS)

auto SteadyNow() {
  return twist::ed::stdlike::steady_clock::now();
}

void ExternalSubmits() {
  executors::ThreadPool pool{4};
  pool.Start();

  twist::test::Repeat repeat;

  while (repeat()) {
    auto start = SteadyNow();

    threads::lockfull::WaitGroup wg;
    wg.Add(3);

    for (size_t i = 0; i < 3; ++i) {
      executors::Submit(pool, [&] {
        twist::ed::stdlike::this_thread::sleep_for(1s);
        wg.Done();
      });
    }

    wg.Wait();

    auto elapsed = SteadyNow() - start;

    ASSERT_TRUE(elapsed < 1500ms);
  }

  fmt::println("Iterations: {}", repeat.IterCount());

  pool.Stop();
}

void InternalSubmits() {
  executors::ThreadPool pool{4};
  pool.Start();

  twist::test::Repeat repeat;

  while (repeat()) {
    auto start = SteadyNow();

    threads::lockfull::WaitGroup wg;
    wg.Add(1);

    executors::Submit(pool, [&] {
      wg.Add(3);

      for (size_t i = 0; i < 3; ++i) {
        executors::Submit(pool, [&] {
          twist::ed::stdlike::this_thread::sleep_for(1s);
          wg.Done();
        });
      }

      wg.Done();
    });

    wg.Wait();

    auto elapsed = SteadyNow() - start;

    ASSERT_TRUE(elapsed < 1500ms);
  }

  fmt::println("Iterations: {}", repeat.IterCount());

  pool.Stop();
}

TEST_SUITE(BalanceTasks) {
  TWIST_TEST(ExternalSubmits, 5s) {
    ExternalSubmits();
  }

  TWIST_TEST(InternalSubmits, 5s) {
    InternalSubmits();
  }
}

#endif

RUN_ALL_TESTS()
