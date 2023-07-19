#include <weave/executors/thread_pool.hpp>

#include <weave/executors/submit.hpp>

#include <weave/fibers/sched/go.hpp>

#include <weave/fibers/sync/mutex.hpp>
#include <weave/fibers/sync/wait_group.hpp>

#include <wheels/core/stop_watch.hpp>

#include <iostream>

using namespace std::chrono_literals;
using namespace weave; // NOLINT

using Scheduler = executors::ThreadPool;

constexpr size_t kThreads = 4;

//////////////////////////////////////////////////////////////////////

void WorkLoadMutexUnstable() {
  const size_t num_clusters = 14;

  for (size_t k = 0; k < num_clusters; ++k) {
    executors::Submit(*Scheduler::Current(),[&] {
      const size_t iters = 100;

      auto mutex = std::make_shared<fibers::Mutex>();

      for (size_t i = 0; i < iters; ++i) {
        executors::Submit(*Scheduler::Current(), [mutex]{
          const size_t critical_sections = 65536;

          for (size_t j = 0; j < critical_sections; ++j) {
            {
              std::lock_guard g(*mutex);
            }
          }
        });
      }
    });
  }
}

//////////////////////////////////////////////////////////////////////

void WorkLoad() {
  wheels::StopWatch sw;

  Scheduler scheduler{kThreads};
  scheduler.Start();

  fibers::Go(scheduler, []() {
    WorkLoadMutexUnstable();
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
