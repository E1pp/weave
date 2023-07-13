#include <weave/executors/submit.hpp>
#include <weave/executors/thread_pool.hpp>

#include <weave/fibers/sched/go.hpp>
#include <weave/fibers/sync/wait_group.hpp>

#include <weave/futures/make/contract.hpp>
#include <weave/futures/make/submit.hpp>
#include <weave/futures/make/value.hpp>

#include <weave/futures/combine/seq/via.hpp>

#include <weave/futures/run/await.hpp>

#include <weave/threads/blocking/wait_group.hpp>

#include <wheels/test/framework.hpp>

#include <twist/rt/run.hpp>
#include <twist/rt/layer/fiber/runtime/scheduler.hpp>

using namespace weave; // NOLINT
using threads::blocking::WaitGroup;

#if !defined(TWIST_FIBERS)

TEST_SUITE(Await) {
  SIMPLE_TEST(JustWorks) {
    executors::ThreadPool scheduler{4};
    WaitGroup main;

    scheduler.Start();

    main.Add(1);

    fibers::Go(scheduler,[&main]{
      auto f = futures::Value(42);
      auto result = std::move(f) | futures::Await();

      WHEELS_ASSERT(*result == 42, "Wrong result!");
      std::cout << "I've read value " << 42 << '\n';

      main.Done();
    });


    main.Wait();
    scheduler.Stop();
  }

  SIMPLE_TEST(AwaitPool) {
    executors::ThreadPool scheduler{4};
    executors::ThreadPool pool{4};
    WaitGroup main;

    scheduler.Start();
    pool.Start();

    main.Add(1);

    fibers::Go(scheduler,[&pool, &main]{
      auto f = futures::Submit(pool, []{
        return result::Ok(42);
      });

      auto result = std::move(f) | futures::Await();

      WHEELS_ASSERT(*result == 42, "Wrong result!");
      std::cout << "I've read value " << 42 << '\n';

      main.Done();
    });


    main.Wait();
    pool.Stop();
    scheduler.Stop();
  }

  SIMPLE_TEST(AwaitOnTheSamePool) {
    executors::ThreadPool scheduler{4};
    WaitGroup main;

    scheduler.Start();

    main.Add(1);

    fibers::Go(scheduler,[&main]{
      auto f = futures::Submit(*(executors::ThreadPool::Current()), []{
        return result::Ok(42);
      });

      auto result = std::move(f) | futures::Await();

      WHEELS_ASSERT(*result == 42, "Wrong result!");
      std::cout << "I've read value " << 42 << '\n';

      main.Done();
    });


    main.Wait();
    scheduler.Stop();
  }

  SIMPLE_TEST(AwaitAnotherFiber) {
    executors::ThreadPool scheduler{4};
    WaitGroup main;

    scheduler.Start();

    main.Add(2);
    auto [f, p] = futures::Contract<int>();

    fibers::Go(scheduler, [p = std::move(p), &main]() mutable{
      std::move(p).SetValue(42);
      main.Done();
    });

    fibers::Go(scheduler, [f = std::move(f), &main]() mutable{
      auto result = std::move(f) | futures::Await();
      
      WHEELS_ASSERT(*result == 42, "Wrong result!");
      std::cout << "I've read value " << 42 << std::endl;
      main.Done();
    });

    main.Wait();
    scheduler.Stop();
  }

  SIMPLE_TEST(AwaitHost) {
   executors::ThreadPool scheduler{4};
    WaitGroup main;

    scheduler.Start();
    main.Add(1);

    auto [f, p] = futures::Contract<int>();

    fibers::Go(scheduler, [f = std::move(f), &main]() mutable{
      auto result = std::move(f) | futures::Await();
      
      WHEELS_ASSERT(*result == 42, "Wrong result!");
      std::cout << "I've read value " << 42 << std::endl;
      main.Done();
    });

    std::move(p).SetValue(42);

    main.Wait();
    scheduler.Stop();
  }
}

#endif

RUN_ALL_TESTS();