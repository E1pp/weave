#include <weave/executors/thread_pool.hpp>

#include <weave/fibers/sched/sleep_for.hpp>

#include <weave/futures/make/after.hpp>
#include <weave/futures/make/just.hpp>
#include <weave/futures/make/never.hpp>
#include <weave/futures/make/submit.hpp>

#include <weave/futures/combine/seq/and_then.hpp>
#include <weave/futures/combine/seq/on_cancel.hpp>
#include <weave/futures/combine/seq/on_success.hpp>
#include <weave/futures/combine/seq/or_else.hpp>
#include <weave/futures/combine/seq/start.hpp>
#include <weave/futures/combine/seq/with_timeout.hpp>

#include <weave/futures/run/await.hpp>
#include <weave/futures/run/detach.hpp>
#include <weave/futures/run/discard.hpp>

#include <weave/threads/lockfull/wait_group.hpp>

#include <weave/timers/processors/standalone.hpp>

#include <wheels/test/framework.hpp>
#include <wheels/test/util/cpu_timer.hpp>

#include <chrono>

#include <fmt/core.h>

using namespace weave; // NOLINT

TEST_SUITE(Futures){
  SIMPLE_TEST(JustAfter){
    timers::StandaloneProcessor proc{};

    proc.MakeGlobal();

    auto start = std::chrono::steady_clock::now();

    futures::After(1s) | futures::AndThen([&](Unit){
      auto finish = std::chrono::steady_clock::now();

      ASSERT_GE(finish - start, 1s);
      ASSERT_LE(finish - start, 1s + 100ms);
    }) | futures::Await();
  }

  SIMPLE_TEST(CancelAfter){
    timers::StandaloneProcessor proc{};

    threads::lockfull::WaitGroup wg;
    wg.Add(1);

    proc.MakeGlobal();

    auto start = std::chrono::steady_clock::now();

    futures::After(1s) | futures::OnSuccess([]{
      ASSERT_TRUE(false);
    }) | futures::OnCancel([&]{
      auto finish = std::chrono::steady_clock::now();

      ASSERT_LE(finish - start, 100ms); 
      wg.Done();     
    }) | futures::Discard();    

    wg.Wait();
  }

  SIMPLE_TEST(CancellableAfter){
    timers::StandaloneProcessor proc{};

    threads::lockfull::WaitGroup wg;
    wg.Add(1);

    proc.MakeGlobal();

    auto start = std::chrono::steady_clock::now();    

    auto f = futures::After(5s) | futures::OnCancel([&]{
      auto finish = std::chrono::steady_clock::now();
      
      fmt::println("Elapsed : {}, expected {}", (finish-start).count(), (1s).count());
      ASSERT_LE(finish - start, 1s + 100ms);
      wg.Done();       
    }) | futures::Start();

    std::this_thread::sleep_for(1s);

    std::move(f).RequestCancel();

    wg.Wait();
  }

  SIMPLE_TEST(DontOvershadow){
    timers::StandaloneProcessor proc{};

    threads::lockfull::WaitGroup wg;
    wg.Add(1);

    proc.MakeGlobal();

    auto start = std::chrono::steady_clock::now();    

    auto f = futures::After(5s) | futures::OnCancel([&]{
      auto finish = std::chrono::steady_clock::now();
      
      fmt::println("Elapsed : {}, expected {}", (finish-start).count(), (1s).count());
      ASSERT_LE(finish - start, 1s + 100ms);
      wg.Done();       
    }) | futures::Start();

    futures::After(2s) | futures::Detach();
    futures::After(3s) | futures::Detach(); 

    std::this_thread::sleep_for(1s);

    std::move(f).RequestCancel();

    wg.Wait();    
  }

  SIMPLE_TEST(TimeoutNever){
    timers::StandaloneProcessor proc{};

    proc.MakeGlobal();

    auto start = std::chrono::steady_clock::now();   

    futures::Never() | futures::WithTimeout(1s) | futures::OrElse([&](Error){
      auto finish = std::chrono::steady_clock::now();

      ASSERT_GE(finish - start, 1s);
      ASSERT_LE(finish - start, 1s + 100ms);       
    }) | futures::Await();
  }

  SIMPLE_TEST(DontStall){
    timers::StandaloneProcessor proc{};

    proc.MakeGlobal();

    auto start = std::chrono::steady_clock::now();

    futures::Just() | futures::WithTimeout(2s) | futures::OrElse([](Error){
      ASSERT_TRUE(false);
    }) | futures::AndThen([&](Unit){
      auto finish = std::chrono::steady_clock::now();

      ASSERT_LE(finish - start, 100ms);
    }) | futures::Await();
  }

  SIMPLE_TEST(CancelSleepFor){
    executors::ThreadPool pool{1};
    pool.Start();

    timers::StandaloneProcessor proc{};
    proc.MakeGlobal();

    threads::lockfull::WaitGroup wg;
    bool flag = false;

    wg.Add(1);

    auto start = std::chrono::steady_clock::now();

    auto f = futures::Submit(pool, [&]{
      wg.Done();
      wheels::Defer log([&]{
        flag = true;
        wg.Done();
      });

      ASSERT_THROW(fibers::SleepFor(5s), cancel::CancelledException);
    }) | futures::Start();

    wg.Wait();

    std::this_thread::sleep_for(1s);

    wg.Add(1);

    std::move(f).RequestCancel();

    wg.Wait();

    auto finish = std::chrono::steady_clock::now();

    ASSERT_GE(finish - start, 1s);
    ASSERT_LE(finish - start, 2s);

    ASSERT_TRUE(flag);

    pool.Stop();
  }
}

RUN_ALL_TESTS()