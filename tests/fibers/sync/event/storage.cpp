#include <twist/test/with/wheels/stress.hpp>

#include <weave/executors/thread_pool.hpp>

#include <weave/fibers/sched/go.hpp>
#include <weave/fibers/sync/event.hpp>

#include <weave/threads/lockfull/wait_group.hpp>

#include <twist/test/repeat.hpp>

using namespace weave; // NOLINT

//////////////////////////////////////////////////////////////////////

void StorageTest() {
  executors::ThreadPool scheduler{5};
  scheduler.Start();

  for (twist::test::Repeat repeat; repeat(); ) {
    threads::lockfull::WaitGroup iter;
    iter.Add(1);

    fibers::Go(scheduler, [&iter] {
      auto* event = new fibers::Event{};

      fibers::Go([event] {
        event->Fire();
      });

      event->Wait();
      delete event;

      iter.Done();
    });

    iter.Wait();
  }

  scheduler.Stop();
}

//////////////////////////////////////////////////////////////////////

TEST_SUITE(Event) {
  TWIST_TEST(Storage, 5s) {
    StorageTest();
  }
}

RUN_ALL_TESTS();
