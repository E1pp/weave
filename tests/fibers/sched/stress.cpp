#include <weave/executors/thread_pool.hpp>

#include <weave/fibers/sched/go.hpp>
#include <weave/fibers/sched/yield.hpp>

#include <weave/threads/blocking/wait_group.hpp>

#include <twist/ed/stdlike/atomic.hpp>

#include <twist/test/with/wheels/stress.hpp>
#include <twist/test/budget.hpp>

using namespace weave; // NOLINT

TEST_SUITE(Fibers) {

  class RacyCounter {
   public:
    void Increment() {
      value_.store(value_.load(std::memory_order_relaxed) + 1,
                   std::memory_order_relaxed);
    }
    size_t Get() const {
      return value_.load(std::memory_order_relaxed);
    }

   private:
    twist::ed::stdlike::atomic<size_t> value_{0};
  };

  TWIST_TEST(RacyCounter, 5s) {
    static const size_t kThreads = 4;
    static const size_t kFibers = 100;

    RacyCounter racy_counter;
    std::atomic<size_t> counter{0};

    executors::ThreadPool scheduler{kThreads};
    scheduler.Start();

    threads::blocking::WaitGroup wg;
    wg.Add(kFibers);

    for (size_t i = 0; i < kFibers; ++i) {
      fibers::Go(scheduler, [&] {
        for (size_t i = 0; twist::test::KeepRunning(); ++i) {
          racy_counter.Increment();
          ++counter;

          if (i % 7 == 0) {
            fibers::Yield();
          }
        }

        wg.Done();
      });
    };

    wg.Wait();

    std::cout << "Threads: " << kThreads << std::endl
              << "Fibers: " << kFibers << std::endl
              << "Increments: " << counter.load() << std::endl
              << "Racy counter value: " << racy_counter.Get() << std::endl;

    ASSERT_LT(racy_counter.Get(), counter.load());

    scheduler.Stop();
  }
}

RUN_ALL_TESTS()
