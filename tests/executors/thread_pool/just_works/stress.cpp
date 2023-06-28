#include <weave/executors/thread_pool.hpp>
#include <weave/executors/submit.hpp>

#include <weave/threads/lockfull/wait_group.hpp>

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

void TestOneTask() {
  executors::ThreadPool pool{4};

  pool.Start();

  for (twist::test::Repeat repeat; repeat(); ) {
    threads::lockfull::WaitGroup wg;
    wg.Add(1);

    executors::Submit(pool, [&] {
      wg.Done();
    });

    wg.Wait();
  }

  pool.Stop();
}

////////////////////////////////////////////////////////////////////////////////

void TestSeries() {
  executors::ThreadPool pool{1};

  pool.Start();

  for (twist::test::Repeat repeat; repeat(); ) {
    const size_t tasks = 1 + repeat.Iter() % 3;

    threads::lockfull::WaitGroup wg;
    wg.Add(tasks);

    for (size_t i = 0; i < tasks; ++i) {
      executors::Submit(pool, [&] {
        wg.Done();
      });
    }

    wg.Wait();
  }

  pool.Stop();
}

////////////////////////////////////////////////////////////////////////////////

void TestCurrent() {
  executors::ThreadPool pool{2};

  pool.Start();

  for (twist::test::Repeat repeat; repeat(); ) {
    threads::lockfull::WaitGroup wg;
    wg.Add(1);

    executors::Submit(pool, [&] {
      executors::Submit(*executors::ThreadPool::Current(), [&] {
        wg.Done();
      });
    });

    wg.Wait();
  }

  pool.Stop();
}

////////////////////////////////////////////////////////////////////////////////

void TestSpawners() {
  executors::ThreadPool pool{4};

  pool.Start();

  twist::test::Repeat repeat;

  while (repeat()) {
    threads::lockfull::WaitGroup wg;

    size_t init = twist::test::Random(1, 10);

    size_t after = twist::test::Random(5);

    wg.Add(init + after);

    for (size_t i = 0; i < init; ++i) {
      size_t spawn;
      if (twist::test::Random2()) {
        spawn = twist::test::Random(1, 100);
      } else {
        spawn = twist::test::Random(3);
      }

      wg.Add(spawn);

      executors::Submit(pool, [&, spawn] {
        // Spawn tasks
        for (size_t j = 0; j < spawn; ++j) {
          executors::Submit(pool, [&wg] {
            wg.Done();
          });
        }

        wg.Done();
      });
    }

    for (size_t k = 0; k < after; ++k) {
      executors::Submit(pool, [&wg] {
        wg.Done();
      });
    }

    wg.Wait();
  }

  fmt::println("Iterations: {}", repeat.Iter());

  pool.Stop();
}

////////////////////////////////////////////////////////////////////////////////

void SubmitFromAnotherThreadTest(){
  executors::ThreadPool pool{4};
  threads::lockfull::WaitGroup wg;

  pool.Start();

  twist::test::Repeat repeat;

  while(repeat()){
    std::deque<std::thread> threads{};

    size_t external = twist::test::Random(1, 5);

    std::atomic<size_t> increments{0};

    size_t local = twist::test::Random(1, 10);

    wg.Add(local + external + 1);

    for(size_t i = 0; i < external; i++){
      threads.emplace_back([&]{
        executors::Submit(pool, [&]{
          increments++;
          wg.Done();
        });
      });
    }

    executors::Submit(pool, [&]{
      for(size_t i = 0; i < local; i++){
        executors::Submit(pool, [&]{
          increments++;
          wg.Done();
        });
      }

      for(auto& thread : threads){
        thread.join();
      }

      threads.clear();
      wg.Done();
    });

    wg.Wait();

    ASSERT_EQ(increments.load(), local + external);
  }

  fmt::println("Iterations: {}", repeat.Iter());

  pool.Stop();
}

}  // namespace tests

////////////////////////////////////////////////////////////////////////////////

TEST_SUITE(ThreadPool) {
  TWIST_TEST(OneTask, 5s) {
    tests::TestOneTask();
  }

  TWIST_TEST(Series, 5s) {
    tests::TestSeries();
  }

  TWIST_TEST(Current, 5s) {
    tests::TestCurrent();
  }

  TWIST_TEST(Spawn, 10s) {
    tests::TestSpawners();
  }

  TWIST_TEST(FromAnotherThread, 5s){
    tests::SubmitFromAnotherThreadTest();
  }
}

RUN_ALL_TESTS()
