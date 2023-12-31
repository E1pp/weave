#include <weave/executors/thread_pool.hpp>
#include <weave/executors/manual.hpp>

#include <weave/futures/make/value.hpp>
#include <weave/futures/make/failure.hpp>
#include <weave/futures/make/submit.hpp>
#include <weave/futures/make/just.hpp>
#include <weave/futures/make/never.hpp>

#include <weave/futures/combine/seq/map.hpp>
#include <weave/futures/combine/seq/and_then.hpp>
#include <weave/futures/combine/seq/or_else.hpp>
#include <weave/futures/combine/seq/flatten.hpp>
#include <weave/futures/combine/seq/flat_map.hpp>
#include <weave/futures/combine/seq/via.hpp>
#include <weave/futures/combine/seq/with_timeout.hpp>

#include <weave/futures/combine/par/first.hpp>

#include <weave/futures/run/detach.hpp>
#include <weave/futures/run/await.hpp>

#include <weave/satellite/satellite.hpp>

#include <weave/result/make/ok.hpp>
#include <weave/result/make/err.hpp>

#include <weave/timers/processors/standalone.hpp>

#include <wheels/test/framework.hpp>

#include "guard.hpp"

#include <thread>
#include <chrono>

#if !defined(TWIST_FIBERS) && !(__has_feature(address_sanitizer) || __has_feature(thread_sanitizer) || defined(__SANITIZE_ADDRESS__))

using namespace weave; // NOLINT

using executors::ThreadPool;

// clang-format off

void WarmUp(ThreadPool& pool, size_t threads){
  for(size_t i = 0 ; i < threads; i++){
    futures::Submit(pool, []{
      satellite::GetExecutor();
      std::this_thread::sleep_for(10ms);
    }) | futures::Detach();
  }

  pool.WaitIdle();
}

TEST_SUITE(AllocFreeFutures) {
  SIMPLE_TEST(AllocationCount) {
    {
      size_t before = AllocationCount();

      int* ptr = new int{42};

      size_t after = AllocationCount();

      size_t count = after - before;
      ASSERT_EQ(count, 1);

      delete ptr;
    }
  }

  SIMPLE_TEST(Value) {
    {
      AllocationGuard do_not_alloc;

      auto r = futures::Value(1) | futures::ThreadAwait();

      ASSERT_TRUE(r);
      ASSERT_EQ(*r, 1);
    }
  }

  SIMPLE_TEST(Submit) {
    ThreadPool pool{4};
    pool.Start();
    // warmup satellite
    WarmUp(pool, 4);

    {
      AllocationGuard do_not_alloc;

      auto f = futures::Submit(pool, [] {
        return result::Ok(17);
      });

      auto r = std::move(f) | futures::ThreadAwait();

      ASSERT_TRUE(r);
      ASSERT_EQ(*r, 17);
    }

    pool.Stop();
  }

  SIMPLE_TEST(Monad) {
    ThreadPool pool{4};
    pool.Start();
    WarmUp(pool, 4);

    {
      AllocationGuard do_not_alloc;

      auto f = futures::Just()
               | futures::Via(pool)
               | futures::AndThen([](Unit) {
                   return result::Ok(1);
                 })
               | futures::Map([](int v) {
                   return v + 1;
                 })
               | futures::OrElse([](Error) {
                   return result::Ok(13);
                 });

      auto r = std::move(f) | futures::ThreadAwait();

      ASSERT_TRUE(r);
      ASSERT_EQ(*r, 2);
    }

    pool.Stop();
  }


  SIMPLE_TEST(Pipeline) {
    ThreadPool pool{4};
    pool.Start();
    WarmUp(pool, 4);

    {
      AllocationGuard do_not_alloc;

      auto f = futures::Just()
               | futures::Via(pool)
               | futures::AndThen([](Unit) {
                   return result::Ok(1);
                 })
               | futures::Map([](int v) {
                   return v + 1;
                 })
               | futures::OrElse([](Error) {
                   return result::Ok(13);
                 })
               | futures::FlatMap([&pool](int v) {
                   return futures::Submit(pool, [v] {
                     return result::Ok(v + 1);
                   });
                 })
               | futures::Map([](int v) {
                   return v + 1;
                 });

      auto r = std::move(f) | futures::ThreadAwait();

      ASSERT_TRUE(r);
      ASSERT_EQ(*r, 4);
    }

    pool.Stop();
  }

  // Broken because of TaggedBuffer!
  SIMPLE_TEST(First){
    ThreadPool pool{4};
    pool.Start();
    WarmUp(pool, 4);

    {
      AllocationGuard do_not_alloc;

      auto f1 = futures::Just()
               | futures::Via(pool)
               | futures::AndThen([](Unit) {
                   return result::Ok(1);
                 })
               | futures::Map([](int v) {
                   return v + 1;
                 })
               | futures::OrElse([](Error) {
                   return result::Ok(13);
                 })
               | futures::FlatMap([&pool](int v) {
                   return futures::Submit(pool, [v] {
                     return result::Ok(v + 1);
                   });
                 })
               | futures::Map([](int v) {
                   return v + 1;
                 });

      auto f2 = futures::Just()
               | futures::Via(pool)
               | futures::AndThen([](Unit) {
                   return result::Ok(1);
                 })
               | futures::Map([](int v) {
                   return v + 1;
                 })
               | futures::OrElse([](Error) {
                   return result::Ok(13);
                 })
               | futures::FlatMap([&pool](int v) {
                   return futures::Submit(pool, [v] {
                     return result::Ok(v + 1);
                   });
                 })
               | futures::Map([](int v) {
                   return v + 1;
                 });

      auto r = futures::no_alloc::First(std::move(f1), std::move(f2)) | futures::ThreadAwait();

      ASSERT_TRUE(r);
      ASSERT_EQ(*r, 4);
    }

    pool.Stop();    
  }

  SIMPLE_TEST(WithTimeout){
    timers::StandaloneProcessor proc{};
    proc.MakeGlobal();

    threads::blocking::WaitGroup wg{};

    wg.Add(1);

    // Warmup of threadlocals 
    {
      futures::Never() | futures::WithTimeout(1ms) | futures::OrElse([&]{
        wg.Done();
      }) | futures::Detach();

      wg.Wait();
    }

    {
      AllocationGuard do_not_alloc{};

      futures::Never() | futures::WithTimeout(1ms) | futures::Await();
    }
  }
}

#endif

RUN_ALL_TESTS()
