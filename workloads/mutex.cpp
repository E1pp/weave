#include <weave/executors/thread_pool.hpp>

#include <weave/executors/submit.hpp>

#include <weave/fibers/sched/go.hpp>
#include <weave/fibers/sched/yield.hpp>

#include <weave/fibers/sync/mutex.hpp>
#include <weave/fibers/sync/wait_group.hpp>

#include <wheels/core/stop_watch.hpp>

#include <iostream>

using namespace std::chrono_literals;
using namespace weave; // NOLINT

using Scheduler = executors::ThreadPool;

constexpr size_t kThreads = 4;

//////////////////////////////////////////////////////////////////////

void WorkLoadMutex() {
  const size_t num_clusters = 14;

  for (size_t k = 0; k < num_clusters; ++k) {
    executors::Submit(*Scheduler::Current(),[&] {
      fibers::WaitGroup wg;

      const size_t iters = 100;
      const size_t critical_sections = 65536;

      size_t cs1 = 0;
      fibers::Mutex mutex1;

      size_t cs2 = 0;
      fibers::Mutex mutex2;

      wg.Add(iters);

      for (size_t i = 0; i < iters; ++i) {
        executors::Submit(*Scheduler::Current(), [&]{
          for (size_t j = 0; j < critical_sections; ++j) {
            {
              std::lock_guard g(mutex1);
              ++cs1;
            }
            if (j % 17 == 0) {
              fibers::Yield();
            }
            {
              std::lock_guard g(mutex2);
              ++cs2;
            }
          }

          wg.Done();
        });

      }

      wg.Wait();

      WHEELS_ASSERT(cs1 == cs2, "Wrong cs's");

    });
  }
}

//////////////////////////////////////////////////////////////////////

void WorkLoad() {
  wheels::StopWatch sw;

  Scheduler scheduler{kThreads};
  scheduler.Start();

  executors::Submit(scheduler, []() {
    WorkLoadMutex();
  });

  scheduler.WaitIdle();
  scheduler.Stop();

  const auto elapsed = sw.Elapsed();

  std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() << "ms " << std::endl;
  scheduler.Metrics().Print();
}

//////////////////////////////////////////////////////////////////////

int main() {
  while (true) {
    WorkLoad();
  }
  
  return 0;
}
