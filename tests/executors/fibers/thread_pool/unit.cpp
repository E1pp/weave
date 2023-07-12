#include <weave/executors/fibers/thread_pool.hpp>
#include <weave/executors/submit.hpp>

#include <weave/fibers/core/fiber.hpp>

#include <weave/fibers/sched/yield.hpp>

#include <weave/fibers/sync/event.hpp>
#include <weave/fibers/sync/mutex.hpp>
#include <weave/fibers/sync/wait_group.hpp>

#include <weave/futures/make/submit.hpp>

#include <weave/futures/combine/seq/and_then.hpp>

#include <weave/futures/run/await.hpp>
#include <weave/futures/run/detach.hpp>
#include <weave/futures/run/get.hpp>

#include <weave/threads/lockfull/wait_group.hpp>

#include <wheels/test/framework.hpp>

#include <wheels/test/util/cpu_timer.hpp>
#include <wheels/core/stop_watch.hpp>

#include <atomic>
#include <chrono>
#include <thread>

#if !defined(TWIST_FIBERS)

using namespace weave; // NOLINT

using namespace std::chrono_literals;

namespace tests{

void ContextCheck(){
  ASSERT_TRUE(fibers::Fiber::Self() != nullptr);
}

TEST_SUITE(ThreadPool) {  
  SIMPLE_TEST(JustWorks) {
    executors::fibers::ThreadPool pool{4};

    pool.Start();

    threads::lockfull::WaitGroup wg;
    wg.Add(1);

    executors::Submit(pool, [&wg] {
      ContextCheck();
      std::cout << "Hello from thread pool!" << std::endl;
      wg.Done();
    });

    wg.Wait();

    pool.Stop();
  }

  SIMPLE_TEST(Wait) {
    executors::fibers::ThreadPool pool{4};

    pool.Start();

    threads::lockfull::WaitGroup wg;
    wg.Add(1);

    executors::Submit(pool, [&] {
      ContextCheck();
      std::this_thread::sleep_for(1s);
      wg.Done();
    });

    wg.Wait();

    pool.Stop();
  }

  SIMPLE_TEST(MultiWait) {
    executors::fibers::ThreadPool pool{1};

    pool.Start();

    for (size_t i = 0; i < 3; ++i) {
      threads::lockfull::WaitGroup wg;
      wg.Add(1);

      executors::Submit(pool, [&] {
        ContextCheck();
        std::this_thread::sleep_for(1s);
        wg.Done();
      });

      wg.Wait();
    }

    pool.Stop();
  }

  SIMPLE_TEST(ManyTasks) {
    executors::fibers::ThreadPool pool{4};

    pool.Start();

    static const size_t kTasks = 17;

    threads::lockfull::WaitGroup wg;
    wg.Add(kTasks);

    for (size_t i = 0; i < kTasks; ++i) {
      executors::Submit(pool, [&] {
        ContextCheck();
        wg.Done();
      });
    }

    wg.Wait();

    pool.Stop();
  }

  TEST(UseThreads, wheels::test::TestOptions().TimeLimit(1s)) {
    executors::fibers::ThreadPool pool{4};

    pool.Start();

    threads::lockfull::WaitGroup wg;
    wg.Add(4);

    for (size_t i = 0; i < 4; ++i) {
      executors::Submit(pool, [&] {
        ContextCheck();
        std::this_thread::sleep_for(750ms);
        wg.Done();
      });
    }

    wg.Wait();

    pool.Stop();
  }

  TEST(TooManyThreads, wheels::test::TestOptions().TimeLimit(2s)) {
    executors::fibers::ThreadPool pool{3};

    pool.Start();

    threads::lockfull::WaitGroup wg;
    wg.Add(4);

    for (size_t i = 0; i < 4; ++i) {
      executors::Submit(pool, [&] {
        ContextCheck();
        std::this_thread::sleep_for(750ms);
        wg.Done();
      });
    }

    wheels::StopWatch stop_watch;

    wg.Wait();

    ASSERT_TRUE(stop_watch.Elapsed() > 1s);

    pool.Stop();
  }

  SIMPLE_TEST(TwoPools) {
    executors::fibers::ThreadPool pool1{1};
    executors::fibers::ThreadPool pool2{1};

    pool1.Start();
    pool2.Start();

    threads::lockfull::WaitGroup wg;
    wg.Add(2);

    wheels::StopWatch stop_watch;

    executors::Submit(pool1, [&] {
      ContextCheck();
      std::this_thread::sleep_for(1s);
      wg.Done();
    });

    executors::Submit(pool2, [&] {
      ContextCheck();
      std::this_thread::sleep_for(1s);
      wg.Done();
    });

    wg.Wait();

    ASSERT_TRUE(stop_watch.Elapsed() < 1500ms);

    pool2.Stop();
    pool1.Stop();
  }

  SIMPLE_TEST(CrossSubmit) {
    executors::fibers::ThreadPool pool1{1};
    executors::fibers::ThreadPool pool2{1};

    pool1.Start();
    pool2.Start();

    threads::lockfull::WaitGroup wg;
    wg.Add(1);

    executors::Submit(pool1, [&]() {
      ContextCheck();
      ASSERT_TRUE(executors::fibers::ThreadPool::Current() == &pool1);
      executors::Submit(pool2, [&]() {
        ContextCheck();
        ASSERT_TRUE(executors::fibers::ThreadPool::Current() == &pool2);
        wg.Done();
      });
    });

    wg.Wait();

    pool1.Stop();
    pool2.Stop();
  }

  SIMPLE_TEST(DoNotBurnCPU) {
    executors::fibers::ThreadPool pool{4};

    pool.Start();

    threads::lockfull::WaitGroup wg;
    wg.Add(4);

    wheels::ProcessCPUTimer cpu_timer;

    // Warmup
    for (size_t i = 0; i < 4; ++i) {
      executors::Submit(pool, [&] {
        ContextCheck();
        std::this_thread::sleep_for(100ms);
        wg.Done();
      });
    }

    wg.Wait();

    pool.Stop();

    ASSERT_TRUE(cpu_timer.Spent() < 100ms);
  }

  SIMPLE_TEST(Current) {
    executors::fibers::ThreadPool pool{1};

    pool.Start();

    ASSERT_EQ(executors::fibers::ThreadPool::Current(), nullptr);

    threads::lockfull::WaitGroup wg;
    wg.Add(1);

    executors::Submit(pool, [&] {
      ContextCheck();
      ASSERT_EQ(executors::fibers::ThreadPool::Current(), &pool);
      wg.Done();
    });

    wg.Wait();

    pool.Stop();
  }

  SIMPLE_TEST(SubmitAfterWait) {
    executors::fibers::ThreadPool pool{4};

    pool.Start();

    threads::lockfull::WaitGroup wg;
    wg.Add(1);

    executors::Submit(pool, [&] {
      ContextCheck();
      std::this_thread::sleep_for(500ms);

      executors::Submit(*executors::fibers::ThreadPool::Current(), [&] {
        ContextCheck();
        std::this_thread::sleep_for(500ms);
        wg.Done();
      });
    });

    wg.Wait();

    pool.Stop();
  }

  SIMPLE_TEST(TaskLifetime) {
    executors::fibers::ThreadPool pool{4};

    pool.Start();

    std::atomic<int> dead{0};

    class Task {
     public:
      explicit Task(std::atomic<int>& done) : counter_(done) {
      }
      Task(const Task&) = delete;
      Task(Task&&) = default;

      ~Task() {
        if (done_) {
          counter_.fetch_add(1);
        }
      }

      void operator()() {
        ContextCheck();
        std::this_thread::sleep_for(100ms);
        done_ = true;
      }

     private:
      bool done_{false};
      std::atomic<int>& counter_;
    };

    for (int i = 0; i < 4; ++i) {
      executors::Submit(pool, Task(dead));
    }
    std::this_thread::sleep_for(1s);
    ASSERT_EQ(dead.load(), 4)

    pool.Stop();
  }

  SIMPLE_TEST(Racy) {
    executors::fibers::ThreadPool pool{4};
    pool.Start();

    static const size_t kTasks = 100500;

    std::atomic<size_t> shared_counter{0};

    threads::lockfull::WaitGroup wg;
    wg.Add(kTasks);

    for (size_t i = 0; i < kTasks; ++i) {
      executors::Submit(pool, [&] {
        ContextCheck();
        int old = shared_counter.load();
        shared_counter.store(old + 1);

        wg.Done();
      });
    }

    wg.Wait();

    std::cout << "Racy counter value: " << shared_counter << std::endl;

    ASSERT_LE(shared_counter.load(), kTasks);

    pool.Stop();
  }
}

///////////////////////////////////////////////////////////////////////////

TEST_SUITE(FibersSched) {
  SIMPLE_TEST(Yield1) {
    executors::fibers::ThreadPool pool{1};
    threads::lockfull::WaitGroup wg;

    wg.Add(1);
    pool.Start();

    bool done = false;

    executors::Submit(pool, [&] {
      fibers::Yield();

      ContextCheck();
      done = true;
      wg.Done();
    });

    wg.Wait();
    ASSERT_TRUE(done);

    pool.Stop();
  }

  SIMPLE_TEST(Yield2) {
    executors::fibers::ThreadPool pool{1};
    threads::lockfull::WaitGroup wg;
    wg.Add(2);

    enum State : int {
      Ping = 0,
      Pong = 1
    };

    int state = Ping;

    executors::Submit(pool, [&] {
      for (size_t i = 0; i < 2; ++i) {
        ASSERT_EQ(state, Ping);
        state = Pong;

        fibers::Yield();
        wg.Done();
      }
    });

    executors::Submit(pool, [&] {
      for (size_t j = 0; j < 2; ++j) {
        ASSERT_EQ(state, Pong);
        state = Ping;

        fibers::Yield();
        wg.Done();
      }
    });

    pool.Start();

    wg.Wait();
    pool.Stop();
  }

  SIMPLE_TEST(Yield3) {
    executors::fibers::ThreadPool pool{4};
    threads::lockfull::WaitGroup wg;
    wg.Add(2);

    static const size_t kYields = 1024;

    auto runner = [&] {
      for (size_t i = 0; i < kYields; ++i) {
        fibers::Yield();
      }
      
      wg.Done();
    };

    executors::Submit(pool, runner);
    executors::Submit(pool, runner);

    pool.Start();

    wg.Wait();
    pool.Stop();
  }

  SIMPLE_TEST(TwoPools) {
    executors::fibers::ThreadPool pool_1{4};
    pool_1.Start();

    executors::fibers::ThreadPool pool_2{4};
    pool_2.Start();

    threads::lockfull::WaitGroup wg;

    auto make_tester = [&wg](executors::fibers::ThreadPool& pool) {
      return [&pool, &wg]() {
        static const size_t kIterations = 128;
        wg.Add(kIterations);

        for (size_t i = 0; i < kIterations; ++i) {
          ContextCheck();
          fibers::Yield();
          
          executors::Submit(pool, [&wg]() {
            ContextCheck();
            wg.Done();
          });
        }

        wg.Done();
      };
    };

    wg.Add(2);
    executors::Submit(pool_1, make_tester(pool_1));
    executors::Submit(pool_2, make_tester(pool_2));

    wg.Wait();

    pool_1.Stop();
    pool_2.Stop();
  }
}

///////////////////////////////////////////////////////////////////////////

TEST_SUITE(FibersEvent){
  SIMPLE_TEST(OneWaiter) {
    executors::fibers::ThreadPool scheduler{4};
    threads::lockfull::WaitGroup wg;

    scheduler.Start();

    static const std::string kMessage = "Hello";

    fibers::Event event;
    std::string data;
    bool ok = false;

    wg.Add(2);

    executors::Submit(scheduler, [&] {
      event.Wait();
      ASSERT_EQ(data, kMessage);
      ok = true;
      wg.Done();
    });

    std::this_thread::sleep_for(1s);

    executors::Submit(scheduler, [&] {
      data = kMessage;
      event.Fire();
      wg.Done();
    });

    wg.Wait();

    ASSERT_TRUE(ok);

    scheduler.Stop();
  }

  SIMPLE_TEST(DoNotBlockThread) {
    executors::fibers::ThreadPool scheduler{1};
    threads::lockfull::WaitGroup wg;

    scheduler.Start();

    fibers::Event event;
    bool ok = false;

    wg.Add(2);

    executors::Submit(scheduler, [&] {
      event.Wait();
      ok = true;
      wg.Done();
    });

    executors::Submit(scheduler, [&] {
      for (size_t i = 0; i < 10; ++i) {
        std::this_thread::sleep_for(32ms);
        fibers::Yield();
      }

      event.Fire();
      wg.Done();
    });

    wg.Wait();

    ASSERT_TRUE(ok);

    scheduler.Stop();
  }

  SIMPLE_TEST(MultipleWaiters) {
    executors::fibers::ThreadPool scheduler{1};
    threads::lockfull::WaitGroup wg;
    scheduler.Start();

    fibers::Event event;
    std::atomic<size_t> waiters{0};

    static const size_t kWaiters = 7;

    wg.Add(kWaiters + 1);
    for (size_t i = 0; i < kWaiters; ++i) {
      executors::Submit(scheduler, [&] {
        event.Wait();
        ++waiters;
        wg.Done();
      });
    }

    std::this_thread::sleep_for(1s);

    executors::Submit(scheduler, [&] {
      event.Fire();
      wg.Done();
    });

    wg.Wait();

    ASSERT_EQ(waiters.load(), kWaiters);

    scheduler.Stop();
  }

  SIMPLE_TEST(DoNotWasteCpu) {
    executors::fibers::ThreadPool scheduler{4};
    threads::lockfull::WaitGroup wg;

    scheduler.Start();

    wheels::ProcessCPUTimer cpu_timer;

    fibers::Event event;

    wg.Add(2);

    executors::Submit(scheduler, [&] {
      event.Wait();
      wg.Done();
    });

    std::this_thread::sleep_for(1s);

    executors::Submit(scheduler, [&] {
      event.Fire();
      wg.Done();
    });

    wg.Wait();

    ASSERT_TRUE(cpu_timer.Spent() < 100ms);

    scheduler.Stop();
  }
}

///////////////////////////////////////////////////////////////////////////

TEST_SUITE(FibersMutex){
  SIMPLE_TEST(JustWorks) {
    executors::fibers::ThreadPool scheduler{4};
    threads::lockfull::WaitGroup wg;
    scheduler.Start();

    fibers::Mutex mutex;
    size_t cs = 0;

    wg.Add(1);
    executors::Submit(scheduler, [&] {
      for (size_t j = 0; j < 11; ++j) {
        std::lock_guard guard(mutex);
        ++cs;
      }
      wg.Done();
    });

    wg.Wait();

    ASSERT_EQ(cs, 11);

    scheduler.Stop();
  }

  SIMPLE_TEST(Counter) {
    executors::fibers::ThreadPool scheduler{4};
    threads::lockfull::WaitGroup wg;
    scheduler.Start();

    fibers::Mutex mutex;
    size_t cs = 0;

    static const size_t kFibers = 10;
    static const size_t kSectionsPerFiber = 1024;

    wg.Add(kFibers);

    for (size_t i = 0; i < kFibers; ++i) {
      executors::Submit(scheduler, [&] {
        for (size_t j = 0; j < kSectionsPerFiber; ++j) {
          std::lock_guard guard(mutex);
          ++cs;
        }

        wg.Done();
      });
    }

    wg.Wait();

    std::cout << "# cs = " << cs
              << " (expected = " << kFibers * kSectionsPerFiber << ")"
              << std::endl;

    ASSERT_EQ(cs, kFibers * kSectionsPerFiber);

    scheduler.Stop();
  }

  SIMPLE_TEST(DoNotWasteCpu) {
    executors::fibers::ThreadPool scheduler{4};
    threads::lockfull::WaitGroup wg;

    scheduler.Start();

    fibers::Mutex mutex;

    wheels::ProcessCPUTimer cpu_timer;

    wg.Add(2);

    executors::Submit(scheduler, [&] {
      mutex.Lock();
      std::this_thread::sleep_for(1s);
      mutex.Unlock();
      wg.Done();
    });

    executors::Submit(scheduler, [&] {
      mutex.Lock();
      mutex.Unlock();
      wg.Done();
    });

    wg.Wait();

    ASSERT_TRUE(cpu_timer.Spent() < 100ms);

    scheduler.Stop();
  }
}

///////////////////////////////////////////////////////////////////////////

TEST_SUITE(FibersWaitGroup){
  SIMPLE_TEST(OneWaiter) {
    executors::fibers::ThreadPool scheduler{/*threads=*/5};
    threads::lockfull::WaitGroup twg;

    scheduler.Start();

    fibers::WaitGroup wg;
    std::atomic<size_t> work{0};
    bool ok = false;

    static const size_t kWorkers = 3;

    wg.Add(kWorkers);
    twg.Add(1 + kWorkers);

    executors::Submit(scheduler, [&] {
      wg.Wait();
      ASSERT_EQ(work.load(), kWorkers);
      ok = true;
      twg.Done();
    });

    for (size_t i = 0; i < kWorkers; ++i) {
      executors::Submit(scheduler, [&] {
        std::this_thread::sleep_for(1s);
        ++work;
        wg.Done();
        twg.Done();
      });
    }

    twg.Wait();

    ASSERT_TRUE(ok);

    scheduler.Stop();
  }

  SIMPLE_TEST(MultipleWaiters) {
    executors::fibers::ThreadPool scheduler{/*threads=*/5};
    threads::lockfull::WaitGroup twg;
    scheduler.Start();

    fibers::WaitGroup wg;

    std::atomic<size_t> work{0};
    std::atomic<size_t> acks{0};

    static const size_t kWorkers = 3;
    static const size_t kWaiters = 4;

    wg.Add(kWorkers);

    twg.Add(kWaiters + kWorkers);

    for (size_t i = 0; i < kWaiters; ++i) {
      executors::Submit(scheduler, [&] {
        wg.Wait();
        ASSERT_EQ(work.load(), kWorkers);
        ++acks;
        twg.Done();
      });
    }

    for (size_t i = 0; i < kWorkers; ++i) {
      executors::Submit(scheduler, [&] {
        std::this_thread::sleep_for(1s);
        ++work;
        wg.Done();
        twg.Done();
      });
    }

    twg.Wait();

    ASSERT_EQ(acks.load(), kWaiters);

    scheduler.Stop();
  }

  SIMPLE_TEST(DoNotBlockThread) {
    executors::fibers::ThreadPool scheduler{1};
    threads::lockfull::WaitGroup twg;
    scheduler.Start();

    fibers::WaitGroup wg;
    bool ok = false;

    twg.Add(2);

    executors::Submit(scheduler, [&] {
      wg.Wait();
      ok = true;

      twg.Done();
    });

    wg.Add(1);

    executors::Submit(scheduler, [&] {
      for (size_t i = 0; i < 10; ++i) {
        std::this_thread::sleep_for(32ms);
        fibers::Yield();
      }
      wg.Done();

      twg.Done();
    });

    twg.Wait();

    ASSERT_TRUE(ok);

    scheduler.Stop();
  }

  SIMPLE_TEST(DoNotWasteCpu) {
    executors::fibers::ThreadPool scheduler{/*threads=*/4};
    threads::lockfull::WaitGroup twg;
    
    scheduler.Start();

    fibers::WaitGroup wg;

    std::atomic<size_t> workers = 0;

    wheels::ProcessCPUTimer cpu_timer;

    static const size_t kWorkers = 3;

    wg.Add(kWorkers);
    twg.Add(kWorkers + 1);

    executors::Submit(scheduler, [&] {
      wg.Wait();
      ASSERT_EQ(workers.load(), kWorkers);
      twg.Done();
    });

    std::this_thread::sleep_for(1s);

    for (size_t i = 0; i < kWorkers; ++i) {
      executors::Submit(scheduler, [&] {
        ++workers;
        wg.Done();
        twg.Done();
      });
    }

    twg.Wait();

    ASSERT_TRUE(cpu_timer.Spent() < 100ms);

    scheduler.Stop();
  }
}

///////////////////////////////////////////////////////////////////////////

TEST_SUITE(Futures){
  SIMPLE_TEST(AndThenYield){
    executors::fibers::ThreadPool scheduler{4};
    size_t num_iter = 0;
    const size_t iter = 128;

    scheduler.Start();

    auto r = futures::Submit(scheduler, [&]{
      for(size_t i = 0; i < iter; i++){

        num_iter++;
        fibers::Yield();
      }

      return result::Ok();
    }) | futures::Get();

    ASSERT_TRUE(r);
    ASSERT_TRUE(num_iter == iter);

    scheduler.Stop();
  }

  SIMPLE_TEST(Await1){
    executors::fibers::ThreadPool scheduler{4};
    threads::lockfull::WaitGroup wg;

    scheduler.Start();

    auto f = futures::Submit(scheduler, []{
      return result::Ok(42);
    });

    wg.Add(1);
    executors::Submit(scheduler, [f = std::move(f), &wg]() mutable {
      auto r = std::move(f) | futures::Await();
      ASSERT_TRUE(*r == 42);
      wg.Done();
    });

    wg.Wait();
    scheduler.Stop();
  }

  SIMPLE_TEST(Await2){
    executors::fibers::ThreadPool scheduler{1};
    threads::lockfull::WaitGroup wg;

    auto f = futures::Submit(scheduler, []{
      return result::Ok(42);
    });

    wg.Add(1);
    executors::Submit(scheduler, [f = std::move(f), &wg]() mutable {
      auto r = std::move(f) | futures::Await();
      ASSERT_TRUE(*r == 42);
      wg.Done();
    });

    scheduler.Start();

    wg.Wait();
    scheduler.Stop();
  }
}

} // namespace tests

#endif

RUN_ALL_TESTS()
