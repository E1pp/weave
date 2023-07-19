#include <weave/executors/thread_pool.hpp>

#include <weave/executors/submit.hpp>

#include <weave/fibers/sched/go.hpp>
#include <weave/fibers/sched/yield.hpp>

#include <weave/futures/make/submit.hpp>

#include <weave/futures/combine/seq/and_then.hpp>
#include <weave/futures/combine/seq/or_else.hpp>

#include <weave/futures/run/detach.hpp>

#include <weave/threads/blocking/wait_group.hpp>

#include <wheels/core/stop_watch.hpp>

#include <iostream>

using namespace std::chrono_literals;
using namespace weave; // NOLINT

using Scheduler = executors::ThreadPool;

constexpr size_t kThreads = 4;

inline std::error_code TimeoutError() {
  return std::make_error_code(std::errc::timed_out);
}

//////////////////////////////////////////////////////////////////////

void WorkLoadFutures(){
  std::shared_ptr<std::atomic<int>> cs = std::make_shared<std::atomic<int>>(0);

  constexpr size_t kIters = 100500;
  
  for(size_t i = 0; i < 14; i++){
    futures::Submit(*Scheduler::Current(), [&, cs]{

      for(size_t i = 0; i < kIters; i++){
        futures::Submit(*Scheduler::Current(), [&, cs]{
          return cs->fetch_add(1);
        }) | futures::AndThen([&, cs](int v){
          return (v + cs->load()) >> 1;
        }) | futures::AndThen([&](int) -> Result<int> {
          return result::Err(TimeoutError());
        }) | futures::AndThen([&](int){
          return std::string("skip this");
        }) | futures::OrElse([i](Error) {
          if(i % 512 == 0){
            fibers::Yield();
          }

          return std::string("OrElse string");
        }) | futures::Detach();
      }

    }) | futures::Detach();
  }
}

//////////////////////////////////////////////////////////////////////

void WorkLoad() {
  wheels::StopWatch sw;

  Scheduler scheduler{kThreads};
  scheduler.Start();

  fibers::Go(scheduler, []() {
    WorkLoadFutures();
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
