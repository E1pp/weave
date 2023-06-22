#include <weave/executors/fibers/thread_pool.hpp>
#include <weave/executors/submit.hpp>

#include <weave/fibers/sync/event.hpp>
#include <weave/fibers/sync/mutex.hpp>
#include <weave/fibers/sync/wait_group.hpp>

#include <weave/threads/lockfull/wait_group.hpp>

#include <twist/test/with/wheels/stress.hpp>

#include <twist/test/plate.hpp>
#include <twist/test/repeat.hpp>
#include <twist/test/random.hpp>

#include <twist/ed/stdlike/atomic.hpp>
#include <twist/ed/stdlike/thread.hpp>

#include <fmt/core.h>

#include <atomic>
#include <chrono>

using namespace weave; // NOLINT
using namespace std::chrono_literals;

//////////////////////////////////////////////////////////////////////

namespace mutex_tests {

void StressTest1(size_t fibers) {
  executors::fibers::ThreadPool scheduler{4};
  scheduler.Start();

  fibers::Mutex mutex;
  twist::test::Plate plate;

  threads::lockfull::WaitGroup wg;
  wg.Add(fibers);

  for (size_t i = 0; i < fibers; ++i) {
    executors::Submit(scheduler, [&] {
      for (twist::test::TimeBudget budget; budget; ) {
        mutex.Lock();
        plate.Access();
        mutex.Unlock();
      }

      wg.Done();
    });
  }

  wg.Wait();

  std::cout << "# critical sections: " << plate.AccessCount() << std::endl;

  scheduler.Stop();
}

//////////////////////////////////////////////////////////////////////

void StressTest2() {
  executors::fibers::ThreadPool scheduler{4};
  scheduler.Start();

  for (twist::test::Repeat repeat; repeat(); ) {
    size_t fibers = 2 + repeat.Iter() % 5;

    threads::lockfull::WaitGroup iter;
    iter.Add(fibers);

    fibers::Mutex mutex;
    std::atomic<size_t> cs{0};

    for (size_t j = 0; j < fibers; ++j) {
      executors::Submit(scheduler, [&] {
        mutex.Lock();
        ++cs;
        mutex.Unlock();

        iter.Done();
      });
    }

    iter.Wait();

    ASSERT_EQ(cs, fibers);
  }

  scheduler.Stop();
}

} // namespace mutex_tests

namespace event_tests {

void StressTest() {
  executors::fibers::ThreadPool scheduler{5};
  scheduler.Start();

  for (twist::test::Repeat repeat; repeat(); ) {
    const size_t waiters = 1 + repeat.Iter() % 4;

    threads::lockfull::WaitGroup iter;
    iter.Add(waiters);

    fibers::Event event;
    bool work = false;
    std::atomic<size_t> acks{0};

    for (size_t i = 0; i < waiters; ++i) {
      executors::Submit(scheduler, [&] {
        event.Wait();
        ASSERT_TRUE(work);
        ++acks;

        iter.Done();
      });
    }

    executors::Submit(scheduler, [&] {
      work = true;
      event.Fire();
    });

    iter.Wait();

    ASSERT_EQ(acks.load(), waiters);
  }

  scheduler.Stop();
}

} // namespace event_tests

namespace wg_tests {

void StressTest() {
  executors::fibers::ThreadPool scheduler{/*threads=*/4};
  scheduler.Start();

  for (twist::test::Repeat repeat; repeat(); ) {
    const size_t workers = 1 + twist::test::Random(3);
    const size_t waiters = 1 + twist::test::Random(3);

    threads::lockfull::WaitGroup iter;
    iter.Add(workers + waiters);

    fibers::WaitGroup wg;

    std::atomic<size_t> work{0};
    std::atomic<size_t> acks{0};

    wg.Add(workers);

    // Waiters

    for (size_t i = 0; i < waiters; ++i) {
      executors::Submit(scheduler, [&] {
        wg.Wait();
        ASSERT_EQ(work.load(), workers);
        ++acks;

        iter.Done();
      });
    }

    // Workers

    for (size_t j = 0; j < workers; ++j) {
      executors::Submit(scheduler, [&] {
        ++work;
        wg.Done();

        iter.Done();
      });
    }

    iter.Wait();

    ASSERT_EQ(acks.load(), waiters);
  }

  scheduler.Stop();
}

} // namespace wg_tests

//////////////////////////////////////////////////////////////////////

TEST_SUITE(Mutex) {
  TWIST_TEST(Stress_1_4, 5s) {
    mutex_tests::StressTest1(/*fibers=*/4);
  }

  TWIST_TEST(Stress_1_16, 5s) {
    mutex_tests::StressTest1(/*fibers=*/16);
  }

  TWIST_TEST(Stress_2, 5s) {
    mutex_tests::StressTest2();
  }
}

TEST_SUITE(Event) {
  TWIST_TEST(Stress, 5s) {
    event_tests::StressTest();
  }
}

TEST_SUITE(WaitGroup) {
  TWIST_TEST(Stress, 5s) {
    wg_tests::StressTest();
  }
}

RUN_ALL_TESTS()