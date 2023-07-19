#include <weave/executors/thread_pool.hpp>

#include <weave/executors/submit.hpp>

#include <weave/fibers/sched/go.hpp>
#include <weave/fibers/sched/yield.hpp>

#include <weave/fibers/sync/select.hpp>

#include <wheels/core/stop_watch.hpp>

#include <iostream>

using namespace std::chrono_literals;
using namespace weave; // NOLINT

using Scheduler = executors::ThreadPool;

constexpr size_t kThreads = 4;

//////////////////////////////////////////////////////////////////////

void WorkLoadChannels() {
  fibers::Channel<int> xs{7};
  fibers::Channel<int> ys{9};

  auto produced = std::make_shared<std::atomic<size_t>>(0);
  auto consumed = std::make_shared<std::atomic<size_t>>(0);

  for (size_t k = 0; k < 50; ++k) {

    const size_t sends = 100'500;

    // Producer
    executors::Submit(*Scheduler::Current(),[xs, ys, produced]() mutable {
      for (size_t i = 0; i < sends; ++i) {
        if (i % 2 == 0) {
          xs.Send(i);
        } else {
          ys.Send(i);
        }
        produced->fetch_add(i);
      }
    });

    // Consumer
    executors::Submit(*Scheduler::Current(),[xs, ys, consumed]() mutable {
      for (size_t i = 0; i < sends; ++i) {
        auto selected = fibers::Select(xs, ys);
        ;
        if (selected.index() == 0) {
          consumed->fetch_add(std::get<0>(selected));
        } else {
          consumed->fetch_add(std::get<1>(selected));
        }
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
    WorkLoadChannels();
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
