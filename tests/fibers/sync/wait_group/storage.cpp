#include <twist/test/with/wheels/stress.hpp>

#include <weave/executors/thread_pool.hpp>

#include <weave/fibers/sched/go.hpp>
#include <weave/fibers/sync/wait_group.hpp>
#include <weave/threads/blocking/wait_group.hpp>

#include <twist/test/repeat.hpp>

using namespace weave; // NOLINT

void StorageTest() {
  executors::ThreadPool scheduler{5};
  scheduler.Start();

  for (twist::test::Repeat repeat; repeat(); ) {
    threads::blocking::WaitGroup iter;
    iter.Add(1);

    fibers::Go(scheduler, [&iter] {
      auto* wg = new fibers::WaitGroup{};

      wg->Add(1);
      fibers::Go([wg] {
        wg->Done();
      });

      wg->Wait();
      delete wg;

      iter.Done();
    });

    iter.Wait();
  }

  scheduler.Stop();
}

#if defined(TWIST_FIBERS)

TEST_SUITE(WaitGroup) {
  TWIST_TEST(Storage, 5s) {
    StorageTest();
  }
}

#endif

RUN_ALL_TESTS();
