#include <weave/executors/thread_pool.hpp>
#include <weave/executors/submit.hpp>

#include <weave/threads/blocking/wait_group.hpp>

#include <twist/test/with/wheels/stress.hpp>

#include <twist/test/race.hpp>
#include <twist/test/budget.hpp>
#include <twist/test/repeat.hpp>
#include <twist/test/yield.hpp>
#include <twist/test/random.hpp>

#include <twist/ed/stdlike/atomic.hpp>
#include <twist/ed/stdlike/thread.hpp>

#include <fmt/core.h>

#include <atomic>

using namespace weave; // NOLINT

namespace tests {

////////////////////////////////////////////////////////////////////////////////

#if defined(__WEAVE_REALTIME__)

void TestSpawners() {
  executors::ThreadPool pool{4};
  auto* logger = pool.GetLogger();

  pool.Start();

  twist::test::Repeat repeat;

  while (repeat()) {
    threads::blocking::WaitGroup wg;

    size_t init = twist::test::Random(1, 10);

    size_t after = twist::test::Random(5);

    std::atomic<size_t> iter{init + after};

    twist::ed::stdlike::thread watcher([&iter, logger]{
        std::vector data = logger->GatherMetrics().Data();

        const size_t target = twist::test::Random(0, data.size() - 1);

        const std::string name = std::get<0>(data[target]);
        size_t prev = std::get<1>(data[target]);

        while(iter.load(std::memory_order::relaxed) != 0){
            auto [new_name, new_count] = logger->GatherMetrics().Data()[target];

            ASSERT_EQ(new_name, name);
            ASSERT_GE(new_count, prev);
            prev = new_count;
        }
    });

    wg.Add(init + after);

    for (size_t i = 0; i < init; ++i) {
      size_t spawn;
      if (twist::test::Random2()) {
        spawn = twist::test::Random(1, 100);
      } else {
        spawn = twist::test::Random(3);
      }
      
      iter.fetch_add(spawn, std::memory_order::relaxed);
      wg.Add(spawn);

      executors::Submit(pool, [&, spawn] {
        // Spawn tasks
        for (size_t j = 0; j < spawn; ++j) {
          executors::Submit(pool, [&wg, &iter] {
            iter.fetch_sub(1, std::memory_order::relaxed);
            wg.Done();
          });
        }

        iter.fetch_sub(1, std::memory_order::relaxed);
        wg.Done();
      });
    }

    for (size_t k = 0; k < after; ++k) {
      executors::Submit(pool, [&wg, &iter] {
        iter.fetch_sub(1, std::memory_order::relaxed);
        wg.Done();
      });
    }

    wg.Wait();
    watcher.join();
  }

  fmt::println("Iterations: {}", repeat.Iter());

  pool.Stop();
}

#endif

}  // namespace tests

////////////////////////////////////////////////////////////////////////////////

#if defined(__WEAVE_REALTIME__)

TEST_SUITE(ThreadPool) {
  TWIST_TEST(SpawnWithLogging, 10s) {
    tests::TestSpawners();
  }
}

#endif

RUN_ALL_TESTS()