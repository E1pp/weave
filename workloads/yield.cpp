#include <weave/executors/thread_pool.hpp>

#include <weave/executors/submit.hpp>

#include <weave/fibers/sched/go.hpp>
#include <weave/fibers/sched/yield.hpp>

#include <wheels/core/stop_watch.hpp>

#include <iostream>

using namespace std::chrono_literals;
using namespace weave; // NOLINT

using Scheduler = executors::ThreadPool;

constexpr size_t kThreads = 4;

//////////////////////////////////////////////////////////////////////

void WorkLoadYield1() {
  for (size_t i = 0; i < 100'000; ++i) {
    fibers::Go([&]() {
      for (size_t j = 0; j < 10; ++j) {
        fibers::Yield();
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
    WorkLoadYield1();
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
