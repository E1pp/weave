#include <wheels/test/framework.hpp>

#include <weave/executors/manual.hpp>

#include <weave/fibers/sched/go.hpp>
#include <weave/fibers/sched/yield.hpp>
#include <weave/fibers/sync/mutex.hpp>

#include <atomic>
#include <chrono>
#include <thread>

#if !defined(TWIST_FIBERS)

using namespace weave; // NOLINT

using namespace std::chrono_literals;

TEST_SUITE(Mutex) {
  SIMPLE_TEST(JustWorks) {
    executors::ManualExecutor scheduler;

    fibers::Mutex mutex;
    size_t cs = 0;

    fibers::Go(scheduler, [&] {
      for (size_t j = 0; j < 11; ++j) {
        std::lock_guard guard(mutex);
        ++cs;
      }
    });

    scheduler.Drain();

    ASSERT_EQ(cs, 11);
  }

  SIMPLE_TEST(Counter) {
    executors::ManualExecutor scheduler;

    fibers::Mutex mutex;
    size_t cs = 0;

    static const size_t kFibers = 10;
    static const size_t kSectionsPerFiber = 1024;

    for (size_t i = 0; i < kFibers; ++i) {
      fibers::Go(scheduler, [&] {
        for (size_t j = 0; j < kSectionsPerFiber; ++j) {
          std::lock_guard guard(mutex);

          ++cs;
          fibers::Yield();
        }
      });
    }

    scheduler.Drain();

    std::cout << "# cs = " << cs
              << " (expected = " << kFibers * kSectionsPerFiber << ")"
              << std::endl;

    ASSERT_EQ(cs, kFibers * kSectionsPerFiber);
  }

  SIMPLE_TEST(SuspendFiber) {
    executors::ManualExecutor scheduler;

    fibers::Mutex mutex;

    fibers::Go(scheduler, [&] {
      mutex.Lock();
      for (size_t i = 0; i < 11; ++i) {
        fibers::Yield();
      }
      mutex.Unlock();
    });

    bool cs = false;

    fibers::Go(scheduler, [&] {
      mutex.Lock();
      cs = true;
      mutex.Unlock();
    });

    {
      size_t tasks = scheduler.RunAtMost(5);
      ASSERT_EQ(tasks, 5);

      ASSERT_FALSE(cs);
    }

    scheduler.Drain();

    ASSERT_TRUE(cs);
  }
}

#endif

RUN_ALL_TESTS()
