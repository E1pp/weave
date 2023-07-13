#include <weave/threads/blocking/wait_group.hpp>

#include <wheels/test/framework.hpp>

#include <twist/rt/run.hpp>
#include <twist/rt/layer/fiber/runtime/scheduler.hpp>

// Assume TWIST_FIBERS=ON

#if defined(TWIST_FIBERS)

TEST_SUITE(WaitGroup) {
  SIMPLE_TEST(Futex) {
    twist::rt::Run([] {
      auto* scheduler = twist::rt::fiber::Scheduler::Current();

      {
        weave::threads::blocking::WaitGroup wg;

        wg.Add(128);

        for (size_t i = 0; i < 128; ++i) {
          wg.Done();
        }

        wg.Wait();
      }

      ASSERT_LT(scheduler->FutexCount(), 10);
    });
  }
}

#endif

RUN_ALL_TESTS()
