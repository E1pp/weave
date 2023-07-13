#include <weave/executors/tp/fast/thread_pool.hpp>
#include <weave/executors/fibers/thread_pool.hpp>

#include <weave/executors/submit.hpp>

#include <weave/fibers/sched/go.hpp>
#include <weave/fibers/sched/yield.hpp>

#include <weave/fibers/sync/mutex.hpp>
#include <weave/fibers/sync/wait_group.hpp>
#include <weave/fibers/sync/select.hpp>

#include <weave/futures/make/submit.hpp>

#include <weave/futures/combine/seq/and_then.hpp>
#include <weave/futures/combine/seq/or_else.hpp>

#include <weave/futures/run/detach.hpp>

#include <weave/threads/blocking/wait_group.hpp>

#include <wheels/core/stop_watch.hpp>

#include <twist/rt/run.hpp>

#include <iostream>

using namespace std::chrono_literals;
using namespace weave; // NOLINT

//using Scheduler = executors::tp::fast::ThreadPool;
using Scheduler = executors::fibers::ThreadPool;

constexpr size_t kThreads = 4;

inline std::error_code TimeoutError() {
  return std::make_error_code(std::errc::timed_out);
}

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

void WorkLoadYield2() {
  fibers::WaitGroup clusters;

  for (size_t i = 0; i < 100000; ++i) {
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

void WorkLoadYield2GoLess() {
  fibers::WaitGroup clusters;

  for (size_t i = 0; i < 10000; ++i) {
    clusters.Add(1);

    executors::Submit(*Scheduler::Current(), [&] {
      fibers::WaitGroup wg;

      const size_t iters = 100;

      wg.Add(iters);

      for(size_t i = 0; i < iters; i++){
        executors::Submit(*Scheduler::Current(), [&]{
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

void WorkLoadRacy(){
  static const size_t kTasks = 100500;

  auto shared_counter = std::make_shared<std::atomic<size_t>>(0);

  for (size_t i = 0; i < kTasks; ++i) {
    fibers::Go([&, shared_counter] {
      int old = shared_counter->load();
      shared_counter->store(old + 1);
    });
  }
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

void WorkLoadBurst(){
  constexpr size_t kBursts = 1000;
  
  for(size_t i = 0; i < kBursts; i++){

    twist::ed::stdlike::this_thread::sleep_for(1ms);

    executors::Submit(*Scheduler::Current(),[&]{

      constexpr size_t kBurstSize = 300;

      for(size_t j = 0; j < kBurstSize; j++){

        executors::Submit(*Scheduler::Current(),[j]{
          if(j % 17 == 0){
            fibers::Yield();
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
    //WorkLoadYield1();
    //WorkLoadYield2();
    //WorkLoadYield2GoLess();
    WorkLoadMutex();
    //WorkLoadMutexUnstable();
    //WorkLoadFutures();
    //WorkLoadRacy();
    //WorkLoadBurst();
    //WorkLoadChannels();
  });

  scheduler.WaitIdle();
  scheduler.Stop();

  const auto elapsed = sw.Elapsed();

  std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() << "ms " << std::endl;
  scheduler.Metrics().Print();
}

//////////////////////////////////////////////////////////////////////

int main() {
  twist::rt::Run([]{
    while (true) {
      WorkLoad();
    }
    
    return 0;
  });
}
