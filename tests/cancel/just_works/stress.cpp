#include <weave/executors/thread_pool.hpp>

#include <weave/fibers/sched/yield.hpp>
#include <weave/fibers/sched/sleep_for.hpp>

#include <weave/futures/make/submit.hpp>

#include <weave/futures/run/await.hpp>

#include <weave/futures/combine/par/all.hpp>
#include <weave/futures/combine/par/first.hpp>

#include <weave/futures/combine/seq/and_then.hpp>
#include <weave/futures/combine/seq/start.hpp>
#include <weave/futures/combine/seq/fork.hpp>
#include <weave/futures/combine/seq/box.hpp>
#include <weave/futures/combine/seq/on_cancel.hpp>

#include <weave/futures/run/detach.hpp>
#include <weave/futures/run/discard.hpp>

#include <weave/timers/processors/standalone.hpp>

#include <wheels/test/framework.hpp>

#include <twist/test/with/wheels/stress.hpp>

#include <twist/test/budget.hpp>

#include <atomic>
#include <chrono>

using namespace weave; // NOLINT
using namespace std::chrono_literals;

using weave::Unit;

//////////////////////////////////////////////////////////////////////

inline std::error_code TimeoutError() {
  return std::make_error_code(std::errc::timed_out);
}

//////////////////////////////////////////////////////////////////////

void StressTestPipeline() {
  executors::ThreadPool pool{4};
  pool.Start();

  size_t iter = 0;

  while (twist::test::KeepRunning()) {
    ++iter;

    size_t pipelines = 1 + iter % 3;

    std::atomic<size_t> counter1{0};
    std::atomic<size_t> counter2{0};
    std::atomic<size_t> counter3{0};
    std::atomic<bool> flag = false;

    for (size_t j = 0; j < pipelines; ++j) {
      auto f = futures::Submit(pool,
                               [&]() {
                                 ++counter1;
                               })
                   | futures::AndThen([&](Unit) { ++counter2;} )
                   | futures::Start()
                   | futures::Via(pool)
                   | futures::AndThen([&](Unit) { ++counter3; })
                   | futures::Start();

      futures::Submit(pool, [f = std::move(f), &flag]() mutable {
        std::move(f).RequestCancel();
        flag.store(true);
      }) | futures::Detach();
    }

    pool.WaitIdle();
    ASSERT_TRUE(flag.load());

    ASSERT_TRUE(counter1.load() >= counter2.load());
    ASSERT_TRUE(counter2.load() >= counter3.load());
  }

  pool.Stop();
}

//////////////////////////////////////////////////////////////////////

void StressTestDDosStart() {
  executors::ThreadPool pool{4};
  pool.Start();

  size_t iter = 0;

  while (twist::test::KeepRunning()) {
    ++iter;

    size_t pipelines = 1 + iter % 3;

    size_t num_attacks = 1 + iter % 15;

    std::atomic<size_t> counter1{0};
    std::atomic<size_t> counter2{0};

    for (size_t j = 0; j < pipelines; ++j) {
      auto f = futures::Submit(pool, [&]{
        ++counter1;

      }) | futures::Start() | futures::AndThen([&, num_attacks]{
        ++counter2;

        for(size_t i = 0; i < num_attacks; ++i){
          try {
            futures::Submit(pool, []{}) | futures::Start() | futures::Await();
          } catch(cancel::CancelledException){
            ASSERT_EQ(iter % 7, 0);
          }
        }
      }) | futures::Start();

      futures::Submit(pool, [f = std::move(f), iter]() mutable {
        if(iter % 7 == 0){
          std::move(f).RequestCancel();
        } else {
          std::move(f) | futures::Await();
        }  
      }) | futures::Detach();

    }

    pool.WaitIdle();

    ASSERT_TRUE(counter1.load() >= counter2.load());
  }

  pool.Stop();
}

//////////////////////////////////////////////////////////////////////

void StressTestParallel() {
  executors::ThreadPool pool{4};
  pool.Start();

  size_t iter = 0;

  while (twist::test::KeepRunning()) {
    ++iter;

    size_t pipelines = 1 + iter % 3;

    std::atomic<size_t> counter1{0};
    std::atomic<size_t> counter2{0};
    std::atomic<size_t> counter3{0};

    for (size_t j = 0; j < pipelines; ++j) {
      auto f1 = futures::Submit(pool, [&]{
        counter1++;
      }) | futures::Start() | futures::AndThen([&]{
        counter3++;
      }) | futures::Start();

      auto f2 = futures::Submit(pool, [&]{
        counter2++;
      }) | futures::Start() | futures::AndThen([&]{
        counter3++;
      }) | futures::Start();

      std::optional<futures::BoxedFuture<Unit>> f{};

      if(iter % 7 == 0){
        f.emplace(futures::First(std::move(f1), std::move(f2)) | futures::Start());
      } else {
        f.emplace(futures::All(std::move(f1), std::move(f2)) | futures::AndThen([]{}) | futures::Start());
      }

      futures::Submit(pool, [&, f = std::move(*f) | futures::Start()]() mutable {
        
        std::move(f).RequestCancel();
      }) | futures::Detach();
    }

    pool.WaitIdle();

    ASSERT_TRUE(counter1.load() + counter2.load() >= counter3.load());
  }

  pool.Stop();
}

//////////////////////////////////////////////////////////////////////

void StressTestDDosParallel() {
  executors::ThreadPool pool{4};
  pool.Start();

  size_t iter = 0;

  while (twist::test::KeepRunning()) {
    ++iter;

    size_t pipelines = 1 + iter % 3;
    size_t num_attacks = 1 + iter % 7;

    std::atomic<size_t> counter1{0};
    std::atomic<size_t> counter2{0};
    std::atomic<size_t> counter3{0};

    for (size_t j = 0; j < pipelines; ++j) {
      auto f1 = futures::Submit(pool, [&, j]{
        if(j % 2 == 0){
          for(size_t k = 0; k < num_attacks / 2; ++k){
            try {
              futures::Submit(pool, []{}) | futures::Start() | futures::Await();
            } catch(cancel::CancelledException){}
          }
        }

        counter1++;
      }) | futures::Start() | futures::AndThen([&, j]{
        if(j % 2 == 1){
          for(size_t k = 0; k < num_attacks / 2; ++k){
            try {
              futures::Submit(pool, []{}) | futures::Start() | futures::Await();
            } catch(cancel::CancelledException){}
          }
        }

        counter3++;
      }) | futures::Start();

      auto f2 = futures::Submit(pool, [&, j]{
        if(j % 2 == 1){
          for(size_t k = 0; k < num_attacks / 2; ++k){
            try {
              futures::Submit(pool, []{}) | futures::Start() | futures::Await();
            } catch(cancel::CancelledException){}
          }
        }

        counter2++;
      }) | futures::Start() | futures::AndThen([&, j]{
        if(j % 2 == 0){
          for(size_t k = 0; k < num_attacks / 2; ++k){
            try {
              futures::Submit(pool, []{}) | futures::Start() | futures::Await();
            } catch(cancel::CancelledException){}
          }
        }

        counter3++;
      }) | futures::Start();

      std::optional<futures::BoxedFuture<Unit>> f{};

      if(iter % 7 == 0){
        f.emplace(futures::First(std::move(f1), std::move(f2)) | futures::Start());
      } else {
        f.emplace(futures::All(std::move(f1), std::move(f2)) | futures::AndThen([]{}) | futures::Start());
      }

      futures::Submit(pool, [&, f = std::move(*f) | futures::Start()]() mutable {
        
        std::move(f).RequestCancel();
      }) | futures::Detach();
    }

    pool.WaitIdle();

    ASSERT_TRUE(counter1.load() + counter2.load() >= counter3.load());
  }

  pool.Stop();
}

//////////////////////////////////////////////////////////////////////

void StressTestFork() {
  executors::ThreadPool pool{4};
  pool.Start();

  size_t iter = 0;

  while (twist::test::KeepRunning()) {
    size_t i = iter++;

    size_t pipelines = 1 + iter % 3;

    for(size_t j = 0; j < pipelines; ++j){
      auto forkable = futures::Submit(pool, [&]{
      }) | futures::Start() | futures::AndThen([&]{
      }) | futures::Start();

      auto left = futures::Submit(pool, [&, i]() -> Status {
        if(i % 5 == 3){
          return result::Err(TimeoutError());
        } else {
          return result::Ok();
        }
      }) | futures::Start();

      auto right = futures::Submit(pool, [&, i]() -> Status {
        if(i % 5 == 0){
          return result::Err(TimeoutError());
        } else {
          return result::Ok();
        }
      }); 

      auto [f1,f2] = std::move(forkable) | futures::Fork<2>();

      auto first1 = futures::First(std::move(f1), std::move(left)) | futures::Start();

      auto first2 = futures::First(std::move(f2), std::move(right)) | futures::Start();

      futures::Submit(pool, [&, i, f1 = std::move(first1), f2 = std::move(first2)]() mutable {
        switch(i % 4){
          case 0:
            std::move(f1).RequestCancel();
            std::move(f2) | futures::Await();
            break;

          case 1:
            std::move(f2).RequestCancel();
            std::move(f1) | futures::Await();
            break;
          
          case 2: {
            auto f = futures::First(std::move(f1), std::move(f2)) | futures::Start();
            std::move(f).RequestCancel();
            break;
          }

          case 3:
            futures::All(std::move(f1), std::move(f2)) | futures::Await();
        }
      }) | futures::Detach();
    }

    pool.WaitIdle();

  }

  fmt::println("Iterations: {}", iter);

  pool.Stop();
}

//////////////////////////////////////////////////////////////////////

void StressTestSched() {
  executors::ThreadPool pool{4};
  pool.Start();

  timers::StandaloneProcessor proc{};
  proc.MakeGlobal();

  size_t iter = 0;

  while (twist::test::KeepRunning()) {
    ++iter;

    size_t pipelines = 1 + iter % 3;

    size_t yields = 1 + iter % 10;

    size_t timeouts = 1 + iter % 5;

    std::atomic<size_t> counter1{0};
    std::atomic<size_t> counter2{0};

    for (size_t j = 0; j < pipelines; ++j) {
      auto f = futures::Submit(pool,[&, yields] {
                                 for(size_t i = 0; i < yields; ++i){
                                  fibers::Yield();
                                 }

                                 ++counter1;
                               })
                   | futures::AndThen([&, timeouts] { 
                    for(size_t i = 0; i < timeouts; ++i){
                      fibers::SleepFor(2ms);
                    }
                    
                    ++counter2;} )
                   | futures::Start();

      futures::Submit(pool, [f = std::move(f)]() mutable {
        std::move(f).RequestCancel();
      }) | futures::Detach();
    }

    pool.WaitIdle();

    ASSERT_TRUE(counter1.load() >= counter2.load());
  }

  pool.Stop();
}

//////////////////////////////////////////////////////////////////////

void StressTestDrop() {
  executors::ThreadPool pool{4};
  pool.Start();

  size_t iter = 0;

  while (twist::test::KeepRunning()) {
    size_t i = iter++;

    size_t pipelines = 1 + iter % 3;

    for(size_t j = 0; j < pipelines; ++j){
      auto forkable = futures::Submit(pool, [&]{
      }) | futures::Start() | futures::AndThen([&]{
      }) | futures::Start();

      auto left = futures::Submit(pool, [&, i]() -> Status {
        if(i % 5 == 3){
          return result::Err(TimeoutError());
        } else {
          return result::Ok();
        }
      });

      auto right = futures::Submit(pool, [&, i]() -> Status {
        if(i % 5 == 0){
          return result::Err(TimeoutError());
        } else {
          return result::Ok();
        }
      }); 

      auto [f1,f2] = std::move(forkable) | futures::Fork<2>();

      auto first1 = futures::First(std::move(f1), std::move(left));

      auto first2 = futures::First(std::move(f2), std::move(right)) | futures::Start();

      futures::Submit(pool, [&, i, f1 = std::move(first1), f2 = std::move(first2)]() mutable {
        if(i % 47 == 23){
          futures::Submit(pool, [&, f1 = std::move(f1), f2 = std::move(f2)]() mutable {
            std::move(f1) | futures::Await();
            std::move(f2) | futures::Await();
          }) | futures::Discard();
        } else {
          futures::Submit(pool, [&, i, f1 = std::move(f1), f2 = std::move(f2)]() mutable {
            switch(i % 4){
              case 0:
                std::move(f1) | futures::Discard();
                std::move(f2) | futures::Await();
                break;

              case 1:
                std::move(f2).RequestCancel();
                std::move(f1) | futures::Await();
                break;
              
              case 2: {
                auto f = futures::First(std::move(f1), std::move(f2)) | futures::Start();
                std::move(f).RequestCancel();
                break;
              }

              case 3:
                futures::All(std::move(f1), std::move(f2)) | futures::Await();
            }
          }) | futures::Detach();
        }
      }) | futures::Detach();
    }

    pool.WaitIdle();

  }

  fmt::println("Iterations: {}", iter);

  pool.Stop();
}

//////////////////////////////////////////////////////////////////////

TEST_SUITE(Cancel) {
  TWIST_TEST(StressPipeline, 5s) {
    StressTestPipeline();
  }

  TWIST_TEST(DDosStart, 5s){
    StressTestDDosStart();
  }

  TWIST_TEST(StressParallel, 5s){
    StressTestParallel();
  }

  TWIST_TEST(DDosParallel, 5s){
    StressTestDDosParallel();
  }

  TWIST_TEST(StressFork, 5s){
    StressTestFork();
  }

  TWIST_TEST(StressSched, 5s){
    StressTestSched();
  }

  TWIST_TEST(StressDrop, 5s){
    StressTestDrop();
  }
}

RUN_ALL_TESTS()