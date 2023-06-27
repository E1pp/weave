#include <weave/executors/fibers/manual.hpp>
#include <weave/executors/submit.hpp>
#include <weave/executors/thread_pool.hpp>

#include <weave/fibers/core/self.hpp>

#include <weave/fibers/sched/yield.hpp>

#include <weave/fibers/sync/channel.hpp>
#include <weave/fibers/sync/mutex.hpp>
#include <weave/fibers/sync/select.hpp>
#include <weave/fibers/sync/wait_group.hpp>

#include <fmt/core.h>

#include <mutex>

using namespace weave; // NOLINT

/*
Fibers -- user-space "thread"

Fibers are stackfull, meaning they have their own call stack. 

Fibers are suspended not stopped, which is a way of saying they
do not block underlying thread.

Since fibers are implemented in user space, they use their own
syncronisation primitives. You must not (!!!) use std::mutex,
std::condition_variable and so in fibers context. Instead,
use primitives from fibers/sync, fibers/sched and futures::Await

Fibers are cooperative -- there is not preemption from scheduler
it can only suspend itself when
*Enters fibers::Yield()
*Enters futures::Await()
*Awaits for sync primitive

Roughly similar to goroutines in golang

Fibers are executed in executors (details on them in examples/executors)
Particularly, they can be used in ManualExecutor on the same thread
which can be very convenient for testing

User should not create nor schedule/run fibers. Instead, use
executors::fibers::ThreadPool (or executors::ThreadPool)
It does everything automatically which turn out to be
almost zero overhead when you don't use fibers' API and
much more effecient when you do (over submitting them manually)
*/

//////////////////////////////////////////////////////////////////////

void YieldExample(){
  fmt::println("Yield example");

  executors::ThreadPool scheduler{1};

  scheduler.Start();

  executors::Submit(scheduler, [&]{
    assert(fibers::IAmFiber());

    executors::Submit(scheduler, [&]{
      for(size_t j = 1; j < 9; j++){
        fmt::println("Fiber 2, Step {}", j);

        // fibers::Yield suspends the current fiber and immediatelly
        // schedules it in the executor. It schedules the fiber as far
        // as possible in the queue of tasks (SchedulerHint::Last)
        // Equivalent to
        // runtime.Gosched() in Golang
        // std::this_thread::yield() in C++

        fibers::Yield();
      }
    });


    for(size_t i = 1; i < 9; i++){
      fmt::println("Fiber 1, Step {}", i);

      // Yield allowing second fiber to be ran
      fibers::Yield();
    }
  });

  scheduler.WaitIdle();

  scheduler.Stop();
}

//////////////////////////////////////////////////////////////////////

void WaitGroupExample(){
  fmt::println("WaitGroup example");

  executors::ThreadPool scheduler{4};

  scheduler.Start();

  executors::Submit(scheduler, [&]{
    assert(fibers::IAmFiber());

    // ~sync.WaitGroup
    // https://gobyexample.com/waitgroups

    fibers::WaitGroup wg;

    std::atomic<size_t> counter{0};

    static const size_t kIterations = 1024;

    wg.Add(kIterations);

    for(size_t i = 0; i < kIterations; i++){
      executors::Submit(scheduler, [&]{
        
        for(size_t j = 0; j < 7; j++){
          counter.fetch_add(1);
          fibers::Yield();
        }

        wg.Done();
      });
    }

    // Wait for other fibers finish work

    wg.Wait();

    fmt::println("Counter is {}, expected {}", counter.load(), 7 * kIterations);
  });

  scheduler.WaitIdle();

  scheduler.Stop(); 
}

//////////////////////////////////////////////////////////////////////

void MutexExample(){
  fmt::println("Mutex example");

  executors::ThreadPool scheduler{4};

  scheduler.Start();

  executors::Submit(scheduler, [&]{
    assert(fibers::IAmFiber());

    fibers::Mutex mutex;

    fibers::WaitGroup wg;

    // Non-atomic counter
    size_t counter{0};

    static const size_t kIterations = 1024;

    wg.Add(kIterations);

    for(size_t i = 0; i < kIterations; i++){
      executors::Submit(scheduler, [&]{
        // Mutex works just like std::mutex
        // However, instead of blocking the thread
        // it suspends the fiber
        {
          std::lock_guard guard(mutex);
          
          // <- We are in a critical section
          ++counter;
        } // <- lock_guard is destroyed here unlocking mutex

        wg.Done();
      });
    }

    // Wait for other fibers finish work
    wg.Wait();

    fmt::println("Counter is {}, expected {}", counter, kIterations);
  });

  scheduler.WaitIdle();

  scheduler.Stop();   
}

//////////////////////////////////////////////////////////////////////

void ChannelExample(){
  fmt::println("Channel Example");

  executors::ThreadPool scheduler{4};

  scheduler.Start();

  executors::Submit(scheduler, [&]{
    assert(fibers::IAmFiber());

    fibers::Channel<int> first{1};
    fibers::Channel<int> second{1};

    static const size_t kIterations = 37;

    // Channels have a shared state thus you have to capture them by value

    executors::Submit(scheduler, [&, first = first, second = second]() mutable{
      for(size_t i = 0; i < kIterations; i++){
        int value = second.Receive();
        fmt::println("Fiber #2 : {}", value);
        first.Send(value + 1);
      }
    });

    second.Send(1);

    for(size_t i = 1; i < kIterations; i++){
      int value = first.Receive();
      fmt::println("Fiber #1 : {}", value);
      second.Send(value + 1);
    }

    first.Receive();
    // Channels reschedule suspended receiver/sender to be ran 
    // as fast as possible on the same thread. (SchedulerHint::Next)
  });

  scheduler.WaitIdle();

  scheduler.Stop();   
}

//////////////////////////////////////////////////////////////////////

void SelectReceiveExample(){
  fmt::println("Select receiving example");

  executors::ThreadPool scheduler{4};

  scheduler.Start();

  executors::Submit(scheduler, [&]{
    assert(fibers::IAmFiber());

    fibers::Channel<int> xs{1};
    fibers::Channel<int> ys{1};

    // Producer #1, sends messages into xs channel
    executors::Submit(scheduler, [xs]() mutable {
      for (int i = 0; i < 32; ++i) {
        xs.Send(i);
      }
    });

    // Producer #2, sends messages into ys channel
    executors::Submit(scheduler, [ys]() mutable {
      for (int j = 0; j < 32; ++j) {
        ys.Send(j);
      }
    });

    // Consumer receives messages from both channels
    for (size_t k = 0; k < 64; ++k) {

      // std::variant selected
      auto selected = fibers::Select(xs, ys);

      switch (selected.index()) {
        case 0:
          // Got a message from xs
          fmt::println("Received X {}", std::get<0>(selected));
          break;
        case 1:
          // Got a message from ys
          fmt::println("Received Y {}", std::get<1>(selected));
          break;
      }
    }

    // Select is not limited to just 2 channels -- you can use a many as you want!

  });

  scheduler.WaitIdle();

  scheduler.Stop();   
}

//////////////////////////////////////////////////////////////////////

void SelectSendExample(){
  fmt::println("Select sending example");

  executors::ThreadPool scheduler{4};

  scheduler.Start();

  executors::Submit(scheduler, [&]{
    assert(fibers::IAmFiber());

    fibers::Channel<int> xs{1};
    fibers::Channel<int> ys{1};

    // Consumer #1, reads messages from xs channel
    executors::Submit(scheduler, [xs]() mutable {
      for (int i = 0; i < 32; ++i) {
        fmt::println("Iter #1 {}", i);
        int value = xs.Receive();
        fmt::println("Consumer X got {}", value);
      }
    });

    // Consumer #2, reads messages from ys channel
    executors::Submit(scheduler, [ys]() mutable {
      for (int j = 0; j < 32; ++j) {
        fmt::println("Iter #2 {}", j);
        int value = ys.Receive();
        fmt::println("Consumer Y got {}", value);
      }
    });

    // Producer sends messages to both channels
    for (size_t k = 0; k < 64; ++k) {
      int value = k;
      // std::variant selected
      auto selected = fibers::Select(fibers::Send(xs, value), fibers::Send(ys, value));

      switch (selected.index()) {
        case 0:
          // Sent a message to xs
          fmt::println("Sent {} to X", value);
          break;
        case 1:
          // Sent a message to ys
          fmt::println("Sent {} to Y", value);
          break;
      }
    }

    // Flush both channels using NonSuspending TrySend
    while(xs.TrySend(42) || ys.TrySend(42)){
      ;
    }
  });

  scheduler.WaitIdle();

  scheduler.Stop();   
}

//////////////////////////////////////////////////////////////////////

void SelectMixedExample(){
  fmt::println("Select mixed example");

  executors::ThreadPool scheduler{4};

  scheduler.Start();

  executors::Submit(scheduler, [&]{
    assert(fibers::IAmFiber());

    fibers::Channel<int> xs{1};
    fibers::Channel<int> ys{1};

    // Consumer, reads messages from xs channel
    executors::Submit(scheduler, [xs]() mutable {
      for (int i = 0; i < 32; ++i) {
        int value = xs.Receive();
        fmt::println("Consumer X got {}", value);
      }
    });

    // Producer, reads messages from ys channel
    executors::Submit(scheduler, [ys]() mutable {
      for (int j = 0; j < 32; ++j) {
        ys.Send(j);
      }
    });

    // We write to first channel and read from the second one
    for (size_t k = 0; k < 64; ++k) {
      int value = k;
      // std::variant selected
      auto selected = fibers::Select(fibers::Send(xs, value), fibers::Receive(ys));

      switch (selected.index()) {
        case 0:
          // Sent a message to xs
          fmt::println("Sent {} to X", value);
          break;
        case 1:
          // Got a message from ys
          fmt::println("Received Y {}", std::get<1>(selected));
          break;
      }
    }

    // Flush channels using non-suspending API
    while(xs.TrySend(42) || ys.TryReceive()){
      ;
    }

  });

  scheduler.WaitIdle();

  scheduler.Stop();   
}

//////////////////////////////////////////////////////////////////////

// Testing part

void ManualExample(){
  // Fibers manual
  executors::fibers::ManualExecutor manual;

  executors::Submit(manual, []{
    fmt::println("Step 1");

    fibers::Yield();

    fmt::println("Step 3");
  });

  executors::Submit(manual, []{
    fmt::println("Step 2");

    fibers::Yield();

    fmt::println("Step 4");
  });

  // We have scheduled two fibers but haven't ran a single one yet

  // Now we run the first fiber until yield
  manual.RunNext();

  fmt::println("After Step 1");

  manual.RunNext();

  fmt::println("After Step 2");

  manual.Drain();

  fmt::println("Step 5");

  /* 
  * We are guaranteed to have execution:
  * Step 1
  * Step 2
  * Step 3
  * Step 4
  * Step 5
  */

  manual.Stop();
}



int main() {
  YieldExample();

  WaitGroupExample();

  MutexExample();

  ChannelExample();

  SelectReceiveExample();
  SelectSendExample();
  SelectMixedExample();

  ManualExample();
}
