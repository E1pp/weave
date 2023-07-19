#include <weave/executors/manual.hpp>

#include <weave/futures/make/contract.hpp>
#include <weave/futures/make/value.hpp>
#include <weave/futures/make/submit.hpp>
#include <weave/futures/make/just.hpp>

#include <weave/futures/combine/seq/and_then.hpp>
#include <weave/futures/combine/seq/box.hpp>
#include <weave/futures/combine/seq/fork.hpp>
#include <weave/futures/combine/seq/on_cancel.hpp>
#include <weave/futures/combine/seq/start.hpp>

#include <weave/futures/combine/par/first.hpp>

#include <weave/futures/run/await.hpp>
#include <weave/futures/run/discard.hpp>
#include <weave/futures/run/detach.hpp>

#include <wheels/test/framework.hpp>
#include <wheels/test/util/cpu_timer.hpp>

#include <chrono>
#include <string>
#include <thread>
#include <tuple>

#if !defined(TWIST_FIBERS)

using namespace weave; // NOLINT

TEST_SUITE(MemoryManagement){
  template <futures::Thunk F>
  void DropFutureTest(F future){
    bool flag = false;

    futures::Just() | futures::AndThen([f = std::move(future)]() mutable{

      std::move(f) | futures::Await();

    }) | futures::OnCancel([&]{

      flag = true;

    }) | futures::Discard();

    ASSERT_TRUE(flag);
  }

  SIMPLE_TEST(DropValue){
    auto f = futures::Value(42);

    DropFutureTest(std::move(f));
  }

  SIMPLE_TEST(DropContract1){
    auto [f, p] = futures::Contract<int>();

    futures::Just() | futures::AndThen([p = std::move(p)]() mutable{
      std::move(p).SetValue(42);
    }) | futures::Discard();

    auto r = std::move(f) | futures::Await();

    ASSERT_FALSE(r);

    ASSERT_EQ(r.error(), futures::Promise<int>::PromiseDead());
  }

  SIMPLE_TEST(DropContract2){
    {    
      auto [f, p] = futures::Contract<int>();

      DropFutureTest(std::move(f));

      std::move(p).SetValue(42);
    }

    {    
      auto [f, p] = futures::Contract<int>();

      std::move(p).SetValue(42);

      DropFutureTest(std::move(f));
    }
  }

  SIMPLE_TEST(DropStart1){
    auto f = futures::Just() | futures::Start();

    DropFutureTest(std::move(f));
  }

  SIMPLE_TEST(DropStart2){
    executors::ManualExecutor manual;

    auto f = futures::Submit(manual, []{}) | futures::Start();

    DropFutureTest(std::move(f));

    manual.Drain();
  }

  SIMPLE_TEST(DropContractStart){
    {    
      auto [f, p] = futures::Contract<int>();

      DropFutureTest(std::move(f) | futures::AndThen([]{}) | futures::Start());

      std::move(p).SetValue(42);
    }

    {    
      auto [f, p] = futures::Contract<int>();

      std::move(p).SetValue(42);

      DropFutureTest(std::move(f) | futures::AndThen([]{}) | futures::Start());
    }
  }

  SIMPLE_TEST(DropStartStart){
    executors::ManualExecutor manual;

    auto f = futures::Submit(manual, []{}) | futures::Start()
                        | futures::AndThen([]{}) | futures::Start();  
    
    DropFutureTest(std::move(f));

    manual.Drain();
  }

  SIMPLE_TEST(DropBoxed){
    auto f = futures::Value(42) | futures::AndThen([]{
    }) | futures::Box();

    DropFutureTest(std::move(f));
  }

  SIMPLE_TEST(DropBoxedContact){
    {    
      auto [f, p] = futures::Contract<int>();

      DropFutureTest(std::move(f) | futures::AndThen([]{}) | futures::Box());

      std::move(p).SetValue(42);
    }

    {    
      auto [f, p] = futures::Contract<int>();

      std::move(p).SetValue(42);

      DropFutureTest(std::move(f) | futures::AndThen([]{}) | futures::Box());
    }    
  }

  SIMPLE_TEST(DropBoxedStart){
    {
      auto f = futures::Just() | futures::Start();

      DropFutureTest(std::move(f) | futures::Box());
    }

    {
      executors::ManualExecutor manual;

      auto f = futures::Submit(manual, []{}) | futures::Start();

      DropFutureTest(std::move(f) | futures::Box());

      manual.Drain();     
    }
  }

  SIMPLE_TEST(DropStartedBox){
    {
      auto f = futures::Just() | futures::Box() | futures::Start();

      DropFutureTest(std::move(f));
    }

    {
      executors::ManualExecutor manual;

      auto f = futures::Submit(manual, []{}) | futures::Box() | futures::Start();

      DropFutureTest(std::move(f));

      manual.Drain();
    }
  }

  SIMPLE_TEST(DropBoxedBoxed){
    auto f = futures::Just() | futures::Box() 
                                   | futures::AndThen([]{ return 42; }) 
                                   | futures::Box();

    DropFutureTest(std::move(f));
  }

  SIMPLE_TEST(DropFork){
    {
      auto [a, b] = futures::Value(42) | futures::Fork<2>();

      DropFutureTest(std::move(a));

      auto r = std::move(b) | futures::Await();

      ASSERT_TRUE(r);

      ASSERT_EQ(*r, 42);
    }

    {
      auto [a, b] = futures::Value(42) | futures::Fork<2>();

      DropFutureTest(std::move(a));

      DropFutureTest(std::move(b));
    }
  }

  SIMPLE_TEST(DropOneCancelAnother){
    executors::ManualExecutor manual;

    auto [a, b] = futures::Submit(manual, []{}) | futures::AndThen([]{}) | futures::Start() | futures::Fork<2>();

    std::move(a) | futures::Discard();

    DropFutureTest(std::move(b));

    manual.Drain();
  }

  // Dropped futures::Fork<N> == | futures::Discard(),
  // so every cancellable future is fine

  SIMPLE_TEST(DropFirst){
    DropFutureTest(futures::First(futures::Value(42), futures::Value(42)));

    std::vector<decltype(futures::Just())> vec{};
    vec.push_back(futures::Just());
    vec.push_back(futures::Just());

    DropFutureTest(futures::First(std::move(vec)));
  }

  SIMPLE_TEST(DropBoxedFirst){
    DropFutureTest(futures::First(futures::Value(42), futures::Value(42)) | futures::Box());

    std::vector<decltype(futures::Just())> vec{};
    vec.push_back(futures::Just());
    vec.push_back(futures::Just());

    DropFutureTest(futures::First(std::move(vec)) | futures::Box());  
  }

  SIMPLE_TEST(DropNoAllocFirst){
    DropFutureTest(futures::no_alloc::First(futures::Value(42), futures::Value(42)));

    std::vector<decltype(futures::Just())> vec{};
    vec.push_back(futures::Just());
    vec.push_back(futures::Just());

    DropFutureTest(futures::no_alloc::First(std::move(vec)));
  }

  SIMPLE_TEST(DropNoAllocBoxedFirst){
    DropFutureTest(futures::no_alloc::First(futures::Value(42), futures::Value(42)) | futures::Box());

    std::vector<decltype(futures::Just())> vec{};
    vec.push_back(futures::Just());
    vec.push_back(futures::Just());

    DropFutureTest(futures::no_alloc::First(std::move(vec)) | futures::Box());  
  }
}

#endif

RUN_ALL_TESTS()