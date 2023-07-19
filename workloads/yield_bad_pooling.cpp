#include <weave/executors/thread_pool.hpp>

#include <weave/executors/submit.hpp>

#include <weave/fibers/sched/go.hpp>
#include <weave/fibers/sched/yield.hpp>

#include <weave/fibers/sync/wait_group.hpp>

#include <wheels/core/stop_watch.hpp>

#include <iostream>

using namespace std::chrono_literals;
using namespace weave; // NOLINT

using Scheduler = executors::ThreadPool;

constexpr size_t kThreads = 4;

//////////////////////////////////////////////////////////////////////

void WorkLoadYield2() {
  fibers::WaitGroup clusters;

  for (size_t i = 0; i < 50000; ++i) {
    clusters.Add(1);

    fibers::Go([&] {
      fibers::WaitGroup wg;

      const size_t iters = 10;

      wg.Add(iters);

      for(size_t i = 0; i < iters; i++){
        fibers::Go([&]{
          fibers::Yield();
          wg.Done();
        });
      }

      wg.Wait();

      clusters.Done();
    });

    clusters.Wait();
  }
}

//////////////////////////////////////////////////////////////////////

void WorkLoad() {
  wheels::StopWatch sw;

  Scheduler scheduler{kThreads};
  scheduler.Start();

  fibers::Go(scheduler, []() {
    WorkLoadYield2();
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
