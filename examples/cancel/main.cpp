#include <weave/executors/thread_pool.hpp>

#include <weave/fibers/sched/sleep_for.hpp>
#include <weave/fibers/sched/yield.hpp>

#include <weave/fibers/sync/mutex.hpp>

#include <weave/futures/make/just.hpp>
#include <weave/futures/make/never.hpp>
#include <weave/futures/make/submit.hpp>
#include <weave/futures/make/value.hpp>

#include <weave/futures/combine/seq/or_else.hpp>
#include <weave/futures/combine/seq/start.hpp>
#include <weave/futures/combine/seq/with_timeout.hpp>

#include <weave/futures/combine/par/first.hpp>

#include <weave/futures/run/await.hpp>
#include <weave/futures/run/detach.hpp>

#include <weave/threads/blocking/wait_group.hpp>

#include <weave/timers/processors/standalone.hpp>

#include <fmt/core.h>

#include <wheels/core/defer.hpp>

#include <mutex>

using namespace weave; // NOLINT

using namespace std::chrono_literals;

// weave lib support cooperative cancellation
// It is mostly transparent but has some exceptions
// and also limitations 

//////////////////////////////////////////////////////////////////////

void FirstCancelExample(){
  fmt::println("First cancel example");

  executors::ThreadPool pool{4};
  pool.Start();
  
  // Never completes
  auto slowpoke = futures::Submit(pool, []{
    wheels::Defer log([]{
      fmt::println("Cancelled");
    });

    while(true){
      fmt::println("Looping...");

      fibers::Yield();
    }

    std::abort();

    return 37;
  });

  auto fast = futures::Value(42);

  auto first = futures::First(std::move(slowpoke), std::move(fast));
  
  // Control is returned immediatelly
  auto result = std::move(first) | futures::Await();

  fmt::println("Value is {}", *result);

  pool.WaitIdle();

  pool.Stop();
}

//////////////////////////////////////////////////////////////////////

void FirstNoAllocExample(){
  fmt::println("First no_alloc example");

  executors::ThreadPool pool{4};
  pool.Start();
  
  // Never completes
  auto slowpoke = futures::Submit(pool, []{
    wheels::Defer log([]{
      fmt::println("Cancelled");
    });

    while(true){
      fmt::println("Looping...");

      fibers::Yield();
    }

    std::abort();

    return 37;
  });

  auto fast = futures::Value(42);

  auto first = futures::no_alloc::First(std::move(slowpoke), std::move(fast));
  
  // Control is returned once slowpoke is cancelled
  auto result = std::move(first) | futures::Await();

  fmt::println("Value is {}", *result);

  pool.Stop();  
}

//////////////////////////////////////////////////////////////////////

void WithTimeoutExample(){
  fmt::println("WithTimeout example");

  executors::ThreadPool pool{4};
  pool.Start();
  
  timers::StandaloneProcessor proc{};
  proc.MakeGlobal();

  auto slowpoke = futures::Submit(pool, []{
    wheels::Defer log([]{
      fmt::println("Cancelled");
    });

    while(true){
      fmt::println("Looping...");
      
      fibers::Yield();
    }

    std::abort();
  });
  
  auto timed_out = std::move(slowpoke) | futures::WithTimeout(100ms);
  
  // no allocations are made in this example
  [[maybe_unused]]auto result = std::move(timed_out) | futures::Await();

  assert(!result.has_value());
  
  pool.Stop();   
}

//////////////////////////////////////////////////////////////////////

void ExplicitCancelExample(){
  fmt::println("Explicit cancel example");

  executors::ThreadPool pool{4};
  pool.Start();

  auto slowpoke = futures::Submit(pool, []{
    wheels::Defer log([]{
      fmt::println("Cancelled");
    });

    while(true){
      fmt::println("Looping...");
      
      fibers::Yield();
    }

    std::abort();
  });
  
  // future starts looping
  auto eager = std::move(slowpoke) | futures::Start();
  
  // Explicitly cancel the future
  std::move(eager).RequestCancel();

  pool.WaitIdle();
  
  pool.Stop();     
}

//////////////////////////////////////////////////////////////////////

// This example is intentionally broken

void DeadlockExample(){
  fmt::println("Deadlock example");

  executors::ThreadPool pool{4};
  pool.Start();

  threads::blocking::WaitGroup wg;

  fibers::Mutex mutex;

  wg.Add(1);
  
  auto slowpoke = futures::Submit(pool, [&]{
    wheels::Defer log([]{
      fmt::println("Cancelled");
    });

    mutex.Lock();
    wg.Done();

    while(true){
      fmt::println("Looping...");
      
      fibers::Yield();
    }

    // if we get cancelled, mutex will not be unlocked!
    mutex.Unlock();
  });
  
  auto eager = std::move(slowpoke) | futures::Start();

  wg.Wait();
  
  // Explicitly cancel
  std::move(eager).RequestCancel();
  
  futures::Submit(pool, [&]{
    // We will deadlock here
    mutex.Lock();
    fmt::println("I'm in a critical section");
    mutex.Unlock();
  }) | futures::Await();

  std::abort(); // Unreachable due to deadlock
  
  pool.Stop();    
}

//////////////////////////////////////////////////////////////////////

// This is exactly the previous example but with RAII used

void RAIIExample(){
  fmt::println("RAII example");

  executors::ThreadPool pool{4};
  pool.Start();

  threads::blocking::WaitGroup wg;

  fibers::Mutex mutex;

  wg.Add(1);
  
  auto slowpoke = futures::Submit(pool, [&]{
    wheels::Defer log([]{
      fmt::println("Cancelled");
    });

    // unlocks in destructor
    std::lock_guard locker(mutex);

    wg.Done();

    while(true){
      fmt::println("Looping...");
      
      fibers::Yield();
    }

  });
  
  auto eager = std::move(slowpoke) | futures::Start();

  wg.Wait();
  
  // Explicitly cancel
  std::move(eager).RequestCancel();
  
  futures::Submit(pool, [&]{
    // No deadlock here
    std::lock_guard lock(mutex);
    fmt::println("I'm in a critical section");

  }) | futures::Await();
  
  pool.Stop();   
}

//////////////////////////////////////////////////////////////////////

void AwaitExample(){
  fmt::println("Await example");

  executors::ThreadPool pool{4};
  pool.Start();

  auto slowpoke = futures::Submit(pool, [&]{
    wheels::Defer log([]{
      fmt::println("Parent cancelled");
    });

    auto eager = futures::Submit(pool, []{
      wheels::Defer log([]{
        fmt::println("Child cancelled");
      });

      while(true){
        fmt::println("Looping...");

        fibers::Yield();
      }

      std::abort(); // Unreachable
    }) | futures::Start();

    std::move(eager) | futures::Await(); // Will be cancelled

    std::abort(); // Unreachable
  }) | futures::Start();
  
  // Manually cancel
  std::move(slowpoke).RequestCancel();

  pool.WaitIdle();
  
  pool.Stop();   
}

//////////////////////////////////////////////////////////////////////

void SleepForExample(){
  fmt::println("SleepFor example");

  executors::ThreadPool pool{1};
  pool.Start();

  timers::StandaloneProcessor proc{};
  proc.MakeGlobal();

  threads::blocking::WaitGroup wg1;
  threads::blocking::WaitGroup wg2;
  bool flag = false;

  auto start = std::chrono::steady_clock::now();

  wg1.Add(1);

  // WG for clean up
  wg2.Add(1);

  auto f = futures::Submit(pool, [&]{
    wg1.Done();
    wheels::Defer cleanup([&]{
      flag = true;
      wg2.Done();
    });

    // We go to sleep not cancelled
    fibers::SleepFor(5s);
  }) | futures::Start();

  wg1.Wait();
  
  // While we sleep, processor takes fiber's timer
  std::this_thread::sleep_for(1s);
  
  // Cancel the sleep
  std::move(f).RequestCancel();
  
  // NB we can't use WaitIdle since there might be some time between us 
  // cancelling sleep_for and thread_pool actually receiving the next part
  wg2.Wait();
  
  assert(flag);

  auto finish = std::chrono::steady_clock::now();

  auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(finish - start);

  fmt::println("Elapsed : {} second(s)", elapsed.count());

  pool.Stop();
}

int main(){
  FirstCancelExample();
  FirstNoAllocExample();

  WithTimeoutExample();
  
  ExplicitCancelExample();

  // DeadlockExample();
  RAIIExample();
  
  AwaitExample();

  SleepForExample();
}