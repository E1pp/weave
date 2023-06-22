#include <twist/test/with/wheels/stress.hpp>

#include <weave/executors/thread_pool.hpp>

#include <weave/fibers/sched/go.hpp>
#include <weave/fibers/sched/yield.hpp>
#include <weave/fibers/sync/event.hpp>

#include <weave/threads/lockfull/wait_group.hpp>

#include <twist/test/repeat.hpp>

using namespace weave; // NOLINT

//////////////////////////////////////////////////////////////////////

void StressTest() {
  executors::ThreadPool scheduler{5};
  scheduler.Start();

  for (twist::test::Repeat repeat; repeat(); ) {
    const size_t waiters = 1 + repeat.Iter() % 4;

    threads::lockfull::WaitGroup iter;
    iter.Add(waiters);

    fibers::Event event;
    bool work = false;
    std::atomic<size_t> acks{0};

    for (size_t i = 0; i < waiters; ++i) {
      fibers::Go(scheduler, [&] {
        event.Wait();
        ASSERT_TRUE(work);
        ++acks;

        iter.Done();
      });
    }

    fibers::Go(scheduler, [&] {
      work = true;
      event.Fire();
    });

    iter.Wait();

    ASSERT_EQ(acks.load(), waiters);
  }

  scheduler.Stop();
}

//////////////////////////////////////////////////////////////////////

TEST_SUITE(Event) {
  TWIST_TEST(Stress, 5s) {
    StressTest();
  }
}

RUN_ALL_TESTS();
