#include <weave/executors/fibers/manual.hpp>

#include <weave/executors/submit.hpp>
#include <weave/executors/thread_pool.hpp>
#include <weave/executors/manual.hpp>

#include <weave/fibers/sched/sleep_for.hpp>
#include <weave/fibers/sched/yield.hpp>

#include <weave/fibers/sync/mutex.hpp>

#include <weave/futures/make/contract.hpp>
#include <weave/futures/make/failure.hpp>
#include <weave/futures/make/value.hpp>
#include <weave/futures/make/submit.hpp>
#include <weave/futures/make/just.hpp>
#include <weave/futures/make/never.hpp>

#include <weave/futures/combine/seq/and_then.hpp>
#include <weave/futures/combine/seq/anyway.hpp>
#include <weave/futures/combine/seq/box.hpp>
#include <weave/futures/combine/seq/flatten.hpp>
#include <weave/futures/combine/seq/fork.hpp>
#include <weave/futures/combine/seq/map.hpp>
#include <weave/futures/combine/seq/on_cancel.hpp>
#include <weave/futures/combine/seq/on_success.hpp>
#include <weave/futures/combine/seq/or_else.hpp>
#include <weave/futures/combine/seq/start.hpp>
#include <weave/futures/combine/seq/via.hpp>

#include <weave/futures/combine/par/all.hpp>
#include <weave/futures/combine/par/first.hpp>
#include <weave/futures/combine/par/quorum.hpp>

#include <weave/futures/run/await.hpp>
#include <weave/futures/run/discard.hpp>
#include <weave/futures/run/detach.hpp>
#include <weave/futures/run/thread_await.hpp>

#include <weave/threads/blocking/stdlike/mutex.hpp>
#include <weave/threads/blocking/wait_group.hpp>

#include <wheels/test/framework.hpp>
#include <wheels/test/util/cpu_timer.hpp>

#include <chrono>
#include <string>
#include <thread>
#include <tuple>

#if !defined(TWIST_FIBERS)

using namespace weave; // NOLINT

using namespace std::chrono_literals;

inline std::error_code TimeoutError() {
  return std::make_error_code(std::errc::timed_out);
}

inline std::error_code IoError() {
  return std::make_error_code(std::errc::io_error);
}

struct MoveOnly {
  MoveOnly() = default;
  MoveOnly(const MoveOnly&) = delete;
  MoveOnly(MoveOnly&&) {};
};

struct NonDefaultConstructible {
  NonDefaultConstructible(int) {}; // NOLINT
};

TEST_SUITE(Sequential){
  SIMPLE_TEST(MakeDiscard){
    futures::Just() | futures::Discard();
    futures::Value(42) | futures::Discard();
    futures::Failure<int>(IoError()) | futures::Discard();
  }

  SIMPLE_TEST(TransformDiscard){
    futures::Just() | futures::Map([](Unit){
      WHEELS_PANIC("Test failed!");
    }) | futures::Discard();

    futures::Just() | futures::AndThen([](Unit){
      WHEELS_PANIC("Test failed!");
    }) | futures::Discard();

    futures::Just() | futures::OrElse([](Error){
      WHEELS_PANIC("Test failed!");
    }) | futures::Discard();
  }

  SIMPLE_TEST(FlattenCancel){
    executors::ManualExecutor manual;
    futures::Value(futures::Submit(manual, []{
      WHEELS_PANIC("Test failed!");
    })) | futures::Flatten() | futures::Discard();

    ASSERT_EQ(manual.Drain(), 0);
  }

  SIMPLE_TEST(CancelContract){
    { 
      auto [f, p] = futures::Contract<int>();
      std::move(f).RequestCancel();

      std::move(p).SetValue(5);
    }

    { 
      auto [f, p] = futures::Contract<int>();
      std::move(p).SetValue(5);

      std::move(f).RequestCancel();
    }
  }

  SIMPLE_TEST(CancelChain) {
    auto [f, p] = futures::Contract<int>();

    std::move(f) | futures::AndThen([](int v){
      return v + 1;
    }) | futures::Discard();

    std::move(p).SetValue(42);
  }

  SIMPLE_TEST(CancelStart){
    executors::ManualExecutor manual;

    auto f = futures::Submit(manual, [&]{
    }) | futures::Map([&](Unit){
      WHEELS_PANIC("Test failed!");
    }) | futures::Start();

    std::move(f).RequestCancel();

    ASSERT_EQ(manual.Drain(), 1);
  }

  SIMPLE_TEST(PassSignal1){
    executors::ManualExecutor manual;

    auto f = futures::Submit(manual, []{

    }) | futures::AndThen([](Unit){
      WHEELS_PANIC("Test failed!");
    }) | futures::Start();

    std::move(f).RequestCancel();

    ASSERT_EQ(manual.Drain(), 1);
  }

  SIMPLE_TEST(PassSignal2){
    executors::ManualExecutor manual;

    auto f = futures::Submit(manual, []{

    }) | futures::AndThen([](Unit){
      WHEELS_PANIC("Test failed!");
    }) | futures::Start() | futures::AndThen([](Unit){
      WHEELS_PANIC("Test failed!");
    }) | futures::Start();

    std::move(f).RequestCancel();

    ASSERT_EQ(manual.Drain(), 1);
  }

  SIMPLE_TEST(CancelBoxedLazy){
    futures::BoxedFuture<Unit> f = futures::Just() | futures::AndThen([](Unit){
      WHEELS_PANIC("Test failed!");
    });

    std::move(f) | futures::AndThen([](Unit){
      WHEELS_PANIC("Test failed!");
    }) | futures::Discard();
  }

  SIMPLE_TEST(CancelBoxedEager){
    auto [f, p] = futures::Contract<Unit>();

    futures::BoxedFuture<Unit> boxed = std::move(f);

    std::move(boxed) | futures::AndThen([](Unit){
      WHEELS_PANIC("boxed : Test failed!");
    }) | futures::Discard();

    std::move(p).Set(result::Ok());
  }

  SIMPLE_TEST(DiscardNever){
    futures::Never() | futures::Discard();
  }

  SIMPLE_TEST(OnCancelJust){
    bool flag = false;

    futures::Just() | futures::OnCancel([&flag]{
      flag = true;
    }) | futures::Discard();

    ASSERT_TRUE(flag);

    futures::Just() | futures::OnCancel([&flag]{
      flag = false;
    }) | futures::Detach();

    ASSERT_TRUE(flag);
  }

  SIMPLE_TEST(OnCancelFailure){
    bool flag = false;

    futures::Failure<Unit>(TimeoutError()) | futures::OnCancel([&flag]{
      flag = true;
    }) | futures::Discard();

    ASSERT_TRUE(flag);

    futures::Failure<Unit>(TimeoutError()) | futures::OnCancel([&flag]{
      flag = false;
    }) | futures::Detach();

    ASSERT_TRUE(flag);
  }

  SIMPLE_TEST(OnCancelVia) {
    executors::ManualExecutor manual;
    bool flag = false;

    futures::Just() | futures::Via(manual) | futures::OnCancel([&flag]{
      flag = true;
    }) | futures::Discard();

    ASSERT_FALSE(flag);

    ASSERT_EQ(manual.Drain(), 1);

    ASSERT_TRUE(flag);

    futures::Just() | futures::Via(manual) | futures::OnCancel([&flag]{
      flag = false;
    }) | futures::Detach();

    ASSERT_TRUE(flag);

    ASSERT_EQ(manual.Drain(), 0);
  }

  SIMPLE_TEST(OnSuccessJust){
    bool flag = true;

    futures::Just() | futures::OnSuccess([&flag]{
      flag = false;
    }) | futures::Discard();

    ASSERT_TRUE(flag);

    futures::Just() | futures::OnSuccess([&flag]{
      flag = false;
    }) | futures::Detach();

    ASSERT_FALSE(flag);
  }

  SIMPLE_TEST(OnSuccessFailure){
    bool flag = true;

    futures::Failure<Unit>(TimeoutError()) | futures::OnSuccess([&flag]{
      flag = false;
    }) | futures::Discard();

    ASSERT_TRUE(flag);

    futures::Failure<Unit>(TimeoutError()) | futures::OnSuccess([&flag]{
      flag = false;
    }) | futures::Detach();

    ASSERT_FALSE(flag);
  }

  SIMPLE_TEST(AnywayJust){
    bool flag = true;

    futures::Just() | futures::Anyway([&flag]{
      flag = false;
    }) | futures::Detach();

    ASSERT_FALSE(flag);

    futures::Just() | futures::Anyway([&flag]{
      flag = true;
    }) | futures::Discard();

    ASSERT_TRUE(flag);
  }

  SIMPLE_TEST(AnywayFailure){
    bool flag = true;

    futures::Failure<Unit>(TimeoutError()) | futures::Anyway([&flag]{
      flag = false;
    }) | futures::Detach();

    ASSERT_FALSE(flag);

    futures::Failure<Unit>(TimeoutError()) | futures::Anyway([&flag]{
      flag = true;
    }) | futures::Discard();

    ASSERT_TRUE(flag);
  }

  SIMPLE_TEST(CancelFork1){
    executors::ManualExecutor manual;

    auto tuple = futures::Submit(manual, []{}) | futures::AndThen([](Unit){
      WHEELS_PANIC("Fork root : test failed!");
    }) | futures::Fork<2>();

    std::apply([](futures::SomeFuture auto&... elements){
      ([](auto one){
        auto eager = std::move(one) | futures::Start();
        std::move(eager).RequestCancel();
      }(std::move(elements)), ...);
    }, tuple);

    ASSERT_EQ(manual.Drain(), 1);
  }

  SIMPLE_TEST(CancelFork2){
    executors::ManualExecutor manual;

    auto tuple = futures::Submit(manual, []{}) | futures::AndThen([](Unit){
      WHEELS_PANIC("Fork root : test failed!");
    }) | futures::Fork<10>();

    std::apply([](futures::SomeFuture auto&... elements){
      ([](auto one){
        auto eager = std::move(one) | futures::Start();
        std::move(eager).RequestCancel();
      }(std::move(elements)), ...);
    }, tuple);

    ASSERT_EQ(manual.Drain(), 1);
  }

  SIMPLE_TEST(ForkCancelTine1){
    auto [f1, f2] = futures::Value(42) | futures::Fork<2>();

    auto ff2 = std::move(f2) | futures::AndThen([](int){
      WHEELS_PANIC("f2 : Test failed!");
    });

    std::move(f1) | futures::Detach();

    std::move(ff2) | futures::Discard();
  }

  SIMPLE_TEST(ForkCancelTine2){
    auto [f1, f2] = futures::Value(42) | futures::Fork<2>();

    auto ff2 = std::move(f2) | futures::AndThen([](int){
      WHEELS_PANIC("f2 : Test failed!");
    });

    std::move(ff2) | futures::Discard();

    auto res = std::move(f1) | futures::Await();

    ASSERT_TRUE(res);

    ASSERT_EQ(*res, 42);
  }
}

TEST_SUITE(Parallel){
  SIMPLE_TEST(FirstShortCircuit){
    auto [f, p] = futures::Contract<int>();
  
    auto deadly_signal = std::move(f) | futures::Map([](int){
      WHEELS_PANIC("deadly_signal : Test failed!");
      return 47;
    });

    futures::First(std::move(deadly_signal), futures::Value(42)) | futures::Detach();

    std::move(p).SetValue(13);
  }

  SIMPLE_TEST(CancelFirst){
    auto f1 = futures::Just() | futures::Map([](Unit){
      WHEELS_PANIC("f1 : Test failed!");
    });

    auto f2 = futures::Just() | futures::Map([](Unit){
      WHEELS_PANIC("f2 : Test failed!");
    });

    futures::First(std::move(f1), std::move(f2)) | futures::AndThen([](Unit){
      WHEELS_PANIC("AndThen : Test failed!");
    }) | futures::Discard();
  }

  SIMPLE_TEST(FirstAndFirst){
    executors::ManualExecutor manual;

    auto f1 = futures::Submit(manual, []{}) | futures::AndThen([](Unit){
      WHEELS_PANIC("f1 : Test failed!");
    });

    auto f2 = futures::Submit(manual, []{}) | futures::AndThen([](Unit){
      WHEELS_PANIC("f2 : Test failed!");
    });

    futures::First(futures::First(std::move(f1), std::move(f2)),  futures::Just()) | futures::Detach();

    ASSERT_EQ(manual.Drain(), 2);
  }

  SIMPLE_TEST(BothShortCircuit){
    auto [f, p] = futures::Contract<int>();

    auto deadly_signal = std::move(f) | futures::Map([](int){
      WHEELS_PANIC("deadly_signal : Test failed!");
    });

    futures::All(std::move(deadly_signal), futures::Failure<int>(TimeoutError())) | futures::Detach();

    std::move(p).SetValue(42);
  }

  SIMPLE_TEST(CancelBoth){
    auto f1 = futures::Just() | futures::Map([](Unit){
      WHEELS_PANIC("f1 : Test failed!");
    });

    auto f2 = futures::Just() | futures::Map([](Unit){
      WHEELS_PANIC("f2 : Test failed!");
    });

    futures::All(std::move(f1), std::move(f2)) | futures::AndThen([](std::tuple<Unit, Unit>){
      WHEELS_PANIC("Both : Test failed!");
    }) | futures::Discard();
  }

  SIMPLE_TEST(BothAndBoth){
    executors::ManualExecutor manual;

    auto f1 = futures::Submit(manual, []{}) | futures::AndThen([](Unit){
      WHEELS_PANIC("f1 : Test failed!");
    });

    auto f2 = futures::Submit(manual, []{}) | futures::AndThen([](Unit){
      WHEELS_PANIC("f2 : Test failed!");
    });

    futures::All(futures::All(std::move(f1), std::move(f2)),  futures::Failure<int>(IoError())) | futures::Detach();

    ASSERT_EQ(manual.Drain(), 2);
  }

  SIMPLE_TEST(QuorumShortCiruit){
    auto f1 = futures::Just();

    auto f2 = futures::Just() | futures::AndThen([](Unit) -> Unit {
      WHEELS_PANIC("f2 : Test failed!");
      return {};
    });

    futures::Quorum(1, std::move(f1), std::move(f2)) | futures::Detach();   
  }

  SIMPLE_TEST(CancelQuorum){
    executors::ManualExecutor manual;

    auto f1 = futures::Submit(manual, []{}) | futures::AndThen([](Unit){
      WHEELS_PANIC("f1 : Test failed!");
    });

    auto f2 = futures::Submit(manual, []{}) | futures::AndThen([](Unit){
      WHEELS_PANIC("f2 : Test failed!");
    }); 

    futures::Quorum(1, std::move(f1), std::move(f2)) | futures::Discard();   
  }

  SIMPLE_TEST(FirstFork){
    auto [f1, f2] = futures::Value(42) | futures::Fork<2>();

    auto res = futures::First(std::move(f1), std::move(f2) | futures::Map([](int){
      WHEELS_PANIC("f2 : Test failed!");
      return 84;
    })) | futures::Await();

    ASSERT_TRUE(res);
    
    ASSERT_EQ(*res, 42);
  }
}

TEST_SUITE(Fibers){
  SIMPLE_TEST(CancelYield1){
    executors::fibers::ManualExecutor manual;

    futures::Submit(manual, []{
      fibers::Yield();
      WHEELS_PANIC("Test failed!");
    }) | futures::Start() | futures::Discard();

    ASSERT_EQ(manual.Drain(), 1);

    manual.Stop();
  }

  SIMPLE_TEST(CancelYield2){
    executors::fibers::ManualExecutor manual;

    auto f1 = futures::Submit(manual, []{
      fibers::Yield();
      WHEELS_PANIC("f1 : Test failed!");
    });

    auto f2 = futures::Submit(manual, []{
      fibers::Yield();
      WHEELS_PANIC("f2 : Test failed!");
    });

    auto f = futures::First(std::move(f1), std::move(f2)) | futures::Start();

    std::move(f) | futures::Discard();

    ASSERT_EQ(manual.Drain(), 2);

    manual.Stop();
  }

  SIMPLE_TEST(Interference){
    executors::fibers::ManualExecutor manual;
    bool flag{false};

    auto f = futures::Submit(manual, [&]{

      futures::Submit(manual, []{}) | futures::Await();

      flag = true;
      fibers::Yield();

      WHEELS_PANIC("Test failed!");
    }) | futures::Start();

    manual.RunAtMost(3);
    ASSERT_TRUE(flag);

    std::move(f).RequestCancel();

    ASSERT_EQ(manual.Drain(), 1);

    manual.Stop();
  }

  SIMPLE_TEST(CancelSleep) {
    executors::fibers::ManualExecutor manual;
    bool flag{false};

    auto f = futures::Submit(manual, [&]{
      ASSERT_THROW(fibers::Yield(), cancel::CancelledException);

      flag = true;

      fibers::SleepFor(1ms);

      WHEELS_PANIC("Test failed!");

    }) | futures::Start();

    manual.RunAtMost(1);

    std::move(f).RequestCancel();

    ASSERT_EQ(manual.Drain(), 1);

    ASSERT_TRUE(flag);

    manual.Stop();    
  }

  SIMPLE_TEST(Propagate1){
    executors::fibers::ManualExecutor manual;

    auto f = futures::Submit(manual, [&]{

      futures::Submit(manual, []{}) | futures::Await();

      WHEELS_PANIC("Test failed!");
    }) | futures::Start();

    std::move(f).RequestCancel();

    manual.Drain();

    manual.Stop();
  }

  SIMPLE_TEST(Propagate2){
    executors::fibers::ManualExecutor manual;
    bool flag = false;

    auto f = futures::Submit(manual, [&]{

      futures::Submit(manual, []{}) | futures::OnCancel([&]{
        flag = true;
      }) | futures::Await();

      WHEELS_PANIC("Test failed!");
    }) | futures::Start();

    std::move(f).RequestCancel();

    manual.Drain();

    ASSERT_TRUE(flag);

    manual.Stop();
  }

  SIMPLE_TEST(Propagate3){
    executors::fibers::ManualExecutor manual;

    bool flag = false;

    auto f = futures::Submit(manual, [&]{
      wheels::Defer log([&]{
        flag = true;
      });

      try {
        fibers::Yield();
      } catch(...){
        // do nothing
      }

      futures::Never() | futures::Await();
      
      WHEELS_PANIC("Unreachable!");
    }) | futures::Start();

    std::move(f).RequestCancel();

    manual.Drain();

    ASSERT_TRUE(flag);

    manual.Stop();
  }

  SIMPLE_TEST(Propagate4){
    executors::fibers::ManualExecutor manual;

    bool flag = false;

    auto f = futures::Submit(manual, [&]{
      wheels::Defer log([&]{
        flag = true;
      });

      try {
        fibers::Yield();
      } catch(...){
        // do nothing
      }

      futures::Never() | futures::Await();
      
      WHEELS_PANIC("Unreachable!");
    });

    futures::First(std::move(f), futures::Just()) | futures::Await();

    manual.Drain();

    ASSERT_TRUE(flag);

    manual.Stop();
  }

  SIMPLE_TEST(Propagate5){
    executors::fibers::ManualExecutor manual;

    bool flag = false;

    auto f = futures::Submit(manual, [&]{
      wheels::Defer log([&]{
        flag = true;
      });

      try {
        fibers::Yield();
      } catch(...){
        // do nothing
      }

      futures::Never() | futures::Await();
      
      WHEELS_PANIC("Unreachable!");
    });

    auto g = futures::Just() | futures::Start();

    futures::First(std::move(f), std::move(g)) | futures::Await();

    manual.Drain();

    ASSERT_TRUE(flag);

    manual.Stop();
  }

  SIMPLE_TEST(DontPropagate1) {
    executors::fibers::ManualExecutor manual;
    bool flag = false;

    futures::Just() | futures::Via(manual) | futures::OnCancel([&]{

      futures::Submit(manual, [&]{
        flag = true;
      }) | futures::Await();

      ASSERT_TRUE(flag);

    }) | futures::Discard();

    ASSERT_FALSE(flag);

    ASSERT_EQ(manual.Drain(), 3);

    manual.Stop();
  }

  SIMPLE_TEST(DontPropagate2) {
    executors::fibers::ManualExecutor manual;
    bool flag = false;

    futures::Just() | futures::Via(manual) | futures::Anyway([&]{

      futures::Submit(manual, [&]{
        flag = true;
      }) | futures::Await();

      ASSERT_TRUE(flag);

    }) | futures::Discard();

    ASSERT_FALSE(flag);

    ASSERT_EQ(manual.Drain(), 3);

    manual.Stop();
  }

  SIMPLE_TEST(DontPropagate3) {
    executors::fibers::ManualExecutor manual;
    bool flag = false;

    auto f = futures::Submit(manual, []{}) | futures::Anyway([&]{

      futures::Submit(manual, [&]{
        flag = true;
      }) | futures::Await();

      ASSERT_TRUE(flag);

    }) | futures::Start();

    ASSERT_FALSE(flag);

    std::move(f).RequestCancel();

    ASSERT_EQ(manual.Drain(), 4);

    manual.Stop();
  }

  SIMPLE_TEST(DontPropagate4) {
    executors::fibers::ManualExecutor manual;
    bool flag = false;

    auto f = futures::Submit(manual, []{}) | futures::OnSuccess([&]{

      futures::Submit(manual, [&]{
        flag = true;
      }) | futures::Await();

      ASSERT_TRUE(flag);

    }) | futures::Start();

    ASSERT_FALSE(flag);

    std::move(f).RequestCancel();

    ASSERT_EQ(manual.Drain(), 4);

    manual.Stop();
  }

  SIMPLE_TEST(TwoLevelDeep){
    executors::ThreadPool pool{4};
    threads::blocking::WaitGroup wg{};

    std::atomic<int> cancelled{0};

    pool.Start();

    wg.Add(1);

    auto grandparent = futures::Submit(pool, [&]{
      wheels::Defer([&]{
        cancelled++;
      });

      auto parent = futures::Submit(pool, [&]{
        wheels::Defer([&]{
          cancelled++;
        });
        
        auto child = futures::Submit(pool, [&]{
          wheels::Defer([&]{
            cancelled++;
          });  

          while(true){
            fibers::Yield();
          }

          ASSERT_TRUE(false);

        }) | futures::Start();

        std::move(child) | futures::Await();

        ASSERT_TRUE(false);

      }) | futures::Start();

      wg.Done();

      std::move(parent) | futures::Await();

      ASSERT_TRUE(false);

    }) | futures::Start();

    wg.Wait();

    std::move(grandparent).RequestCancel();

    pool.WaitIdle();

    ASSERT_EQ(cancelled.load(), 3);

    pool.Stop();
  }

  SIMPLE_TEST(DdosEager1){
    executors::fibers::ManualExecutor manual;

    auto f = futures::Submit(manual, [&]{
    }) | futures::Start() | futures::AndThen([]{

      ASSERT_THROW(futures::Never() | futures::Await(), cancel::CancelledException);
    }) | futures::Start();

    ASSERT_EQ(manual.RunAtMost(2), 2);

    ASSERT_TRUE(manual.IsEmpty());

    std::move(f).RequestCancel();

    ASSERT_TRUE(manual.NonEmpty());

    ASSERT_EQ(manual.Drain(), 1);

    manual.Stop();
  }

  SIMPLE_TEST(DdosEager2){
    executors::fibers::ManualExecutor manual;

    auto f = futures::Submit(manual, [&]{
    }) | futures::Start() | futures::AndThen([]{
      futures::Just() | futures::Start() | futures::Await();
    }) | futures::Start();

    ASSERT_EQ(manual.RunAtMost(1), 1);

    std::move(f) | futures::Detach();

    ASSERT_TRUE(manual.NonEmpty());

    ASSERT_EQ(manual.Drain(), 2);

    manual.Stop();
  }

  SIMPLE_TEST(DdosEager3){
    executors::ThreadPool pool{4};
    pool.Start();

    const size_t num_attacks = 15;

    futures::Submit(pool, [&]{
    }) | futures::Start() | futures::AndThen([&]{
      for(size_t i = 0; i < num_attacks; ++i){
        futures::Submit(pool, []{
        }) | futures::Start() | futures::Await();
      }

    }) | futures::Start() | futures::Await();

    pool.Stop();
  }
}

#endif

RUN_ALL_TESTS()