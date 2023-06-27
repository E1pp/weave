#include <weave/executors/manual.hpp>
#include <weave/executors/strand.hpp>
#include <weave/executors/submit.hpp>
#include <weave/executors/thread_pool.hpp>

// Futures API example
#include <weave/futures/make/submit.hpp>
#include <weave/futures/run/await.hpp>

// Fiber pool example
#include <weave/executors/fibers/manual.hpp>
#include <weave/fibers/core/self.hpp>
#include <weave/fibers/sched/yield.hpp>

#include <fmt/core.h>

#include <string>

using namespace std::chrono_literals;
using namespace weave; // NOLINT

//////////////////////////////////////////////////////////////////////
/*
Executor is an object which executes tasks.

Public API allows these tasks to be lambdas exclusively.

There are two ways if submitting tasks: 

executors::Submit and futures::Submit.

While the latter is preferred, we will take a look at both of them
*/

//////////////////////////////////////////////////////////////////////

void ThreadPoolExample(){
  fmt::println("ThreadPool Example");

  // compute::ThreadPool -- simple pool of worker threads
  // which have shared unbounded blocking queue
  // Best fit for cpu bound tasks 

  executors::tp::compute::ThreadPool pool{/*threads=*/ 4};

  // Pool needs to be started explicitly
  pool.Start();

  static const size_t kIncrements = 100500;

  std::atomic<uint64_t> count{0};

  // Submit lambdas which perform a non-atomic increment
  // They will be performed concurrently by 4 threads
  for(size_t i = 0; i < kIncrements; i++){
    executors::Submit(pool, [&]{
      count.store(count.load() + 1);
    });
  }

  // Wait for pool to finish all the work
  pool.WaitIdle();

  fmt::println("Count is {}, expected is {}", count.load(), kIncrements);

  // ~= std::thread::join() but for pool
  pool.Stop();
}

//////////////////////////////////////////////////////////////////////

void FuturesSubmitExample(){
  fmt::println("Submit example");

  executors::tp::compute::ThreadPool pool{/*threads=*/ 4};

  pool.Start();

  // The most efficient way of using executors is via futures API
  // You can submit a lambda using futures::Submit function
  auto future = futures::Submit(pool, []{
    fmt::println("Task is running...");
    return 42;
  });

  // weave futures are lazy by default. 
  // This means, that the task is not submitted to the pool right here

  pool.WaitIdle(); // <- Returns control instantly

  // To submit a task and also retrieve the value from future
  // One can use futures::Await
  
  auto result = std::move(future) | futures::Await();

  // At this point task is not only submitted but is also finished

  fmt::println("Value is {}", *result);

  pool.Stop(); 
}

//////////////////////////////////////////////////////////////////////

// This example is intentionally broken

void LifeTimeExample(){
  fmt::println("Lifetime example");

  executors::tp::compute::ThreadPool pool{/*threads=*/ 4};

  pool.Start();  

  // futures::Submit as well as executors::Submit transfers lambda's
  // ownership to the executor. However, if lambda has some non-owning references,
  // executor will not obtain ownership over the referred object
  {
    std::string hello{"Hello!"};

    executors::Submit(pool, [&hello]{
      // Stall the executor
      std::this_thread::sleep_for(1s);

      // Heap-use-after-scope right here
      fmt::println("String is \"{}\"", hello);
    });
  } // <- hello is destroyed

  pool.WaitIdle();

  pool.Stop();
}

//////////////////////////////////////////////////////////////////////

void StrandExample(){
  fmt::println("Strand example");

  executors::tp::compute::ThreadPool pool{/*threads=*/ 4};

  pool.Start();  

  // Strand is an executor which guarantees that tasks submitted into it
  // will be executed in a sequence with happens-before ordering
  // Strand is not a standalone executor
  // Instead Strand is a decorator over the normal executor

  executors::Strand strand{pool};

  static const size_t kConcurrentStrandUsers = 5;

  static const size_t kIncrements = 100500;

  size_t counter{0};

  // We shall submit tasks into the strand from thread_pool workers
  // This way tasks would be submitted concurrently 
  // And yet strand would execute them in a sequence

  for(size_t i = 0; i < kConcurrentStrandUsers; i++){
    executors::Submit(pool, [&]{
      for(size_t j = 0; j < kIncrements; j++){
        executors::Submit(strand, [&]{
          // This increment is non-atomic
          // However, it is performed inside the strand
          // so every increment will be ordered
          counter++;
        });
      }
    });
  }

  pool.WaitIdle();

  fmt::println("Increments {}, expected {}", counter, kConcurrentStrandUsers * kIncrements);

  pool.Stop();
}

//////////////////////////////////////////////////////////////////////

void ManualExample(){
  fmt::println("Manual example");

  // ManualExecutor -- single thread executor which runs tasks
  // on command
  executors::ManualExecutor manual;

  executors::Submit(manual, []{
    fmt::println("Hello from manual!");
  });

  fmt::println("Currently manual has {} tasks", manual.TaskCount());

  // We can launch tasks using RunNext, RunAtMost and Drain methods

  // RunNext launches 1 task if there is one and returns true iff a task was ran

  manual.RunNext(); // return true

  // Let's add some more tasks to the manual

  for(size_t i = 0; i < 15; i++){
    executors::Submit(manual, [i]{
      fmt::println("Running task #{}", i);
    });
  }

  // RunAtMost(count) runs no more than count tasks as the name suggests
  fmt::println("Ran tasks: {}", manual.RunAtMost(5)); // 5

  // Drain runs tasks until the queue is empty
  fmt::println("Ran tasks {}", manual.Drain()); // 10

  // Drain accounts for tasks which submit tasks themselves
  executors::Submit(manual, [&]{
    executors::Submit(manual, []{
      fmt::println("Hello!");
    });
  });

  fmt::println("Ran tasks {}", manual.Drain()); // 2
}

//////////////////////////////////////////////////////////////////////

// Consider checking these examples after you are familiar with fiber's API
// examples/fibers 

void FiberManualExample(){
  // Fiber manual executor == manual which launches tasks in fibers context
  executors::fibers::ManualExecutor manual;

  executors::Submit(manual, []{
    // We are in fibers context!
    assert(fibers::IAmFiber());

    for(size_t i = 0; i < 10; i++){
      fibers::Yield();
    }

  });

  fmt::println("Tasks ran: {}", manual.Drain()); // 11

  // There is a difference in API 
  // fibers Manual has to be stopped manually
  manual.Stop();
}

//////////////////////////////////////////////////////////////////////

void FiberThreadPoolExample() {
  // ThreadPool also has a fiber version
  // Currently fiber threadpool (actually scheduler) 
  // is the default version of threadpool form executors/thread_pool.hpp
  executors::fibers::ThreadPool fiber_pool{/*threads=*/ 4};

  fiber_pool.Start();

  executors::Submit(fiber_pool, []{
    assert(fibers::IAmFiber());

    for(size_t i = 0; i < 25; i++){
      // fiber version support workstealing thus iterations
      // may be ran on different threads
      fmt::println("Worker {} did step {}", executors::tp::fast::Worker::Current()->Index(), i);

      fibers::Yield();
    }
  });

  fiber_pool.WaitIdle();

  fiber_pool.Stop();
}

int main() {
  ThreadPoolExample();
  FuturesSubmitExample();

  //LifeTimeExample();

  StrandExample();
  ManualExample();

  FiberManualExample();
  FiberThreadPoolExample();
}
