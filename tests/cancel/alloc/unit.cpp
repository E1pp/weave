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
#include <weave/futures/combine/par/select.hpp>

#include <weave/futures/run/await.hpp>
#include <weave/futures/run/discard.hpp>
#include <weave/futures/run/detach.hpp>
#include <weave/futures/run/get.hpp>

#include <weave/threads/lockfull/stdlike/mutex.hpp>
#include <weave/threads/lockfull/wait_group.hpp>

#include <wheels/test/framework.hpp>
#include <wheels/test/util/cpu_timer.hpp>

#include <chrono>
#include <string>
#include <thread>
#include <tuple>

using namespace weave; // NOLINT

inline std::error_code TimeoutError() {
  return std::make_error_code(std::errc::timed_out);
}

inline std::error_code IoError() {
  return std::make_error_code(std::errc::io_error);
}

TEST_SUITE(Futures) {
  SIMPLE_TEST(FirstOk1) {
    auto [f1, p1] = futures::Contract<int>();
    auto [f2, p2] = futures::Contract<int>();

    auto first = futures::no_alloc::First(std::move(f1) | futures::AndThen([](int){
      WHEELS_PANIC("Don't run me");
      return 5;
    }), std::move(f2));

    bool ok = false;

    std::move(first)
        | futures::Map([&ok](int v) {
            ASSERT_EQ(v, 2);
            ok = true;
            return Unit{};
          })
        | futures::Detach();

    std::move(p2).SetValue(2);

    std::move(p1).SetValue(1);

    ASSERT_TRUE(ok);
  }

  SIMPLE_TEST(FirstOk2) {
    auto [f1, p1] = futures::Contract<int>();
    auto [f2, p2] = futures::Contract<int>();

    auto first = futures::no_alloc::First(std::move(f1), std::move(f2));

    bool ok = false;

    std::move(first)
        | futures::Map([&ok](int v) {
            ASSERT_EQ(v, 29);
            ok = true;
            return Unit{};
          })
        | futures::Detach();

    std::move(p1).SetError(TimeoutError());
    std::move(p2).SetValue(29);

    ASSERT_TRUE(ok);
  }

  SIMPLE_TEST(FirstOk3) {
    auto [f1, p1] = futures::Contract<int>();
    auto [f2, p2] = futures::Contract<int>();

    auto first = futures::no_alloc::First(std::move(f1), std::move(f2));

    bool ok = false;

    std::move(first)
        | futures::Map([&ok](int v) {
            ASSERT_EQ(v, 31);
            ok = true;
            return Unit{};
          })
        | futures::Detach();

    std::move(p2).SetError(IoError());
    std::move(p1).SetValue(31);

    ASSERT_TRUE(ok);
  }

  SIMPLE_TEST(FirstFailure) {
    auto [f1, p1] = futures::Contract<int>();
    auto [f2, p2] = futures::Contract<int>();

    auto first = futures::no_alloc::First(std::move(f1), std::move(f2));

    bool fail = false;

    std::move(first)
        | futures::OrElse([&](Error e) -> Result<int> {
            ASSERT_EQ(e, TimeoutError());
            fail = true;
            return result::Err(e);
          })
        | futures::Detach();

    std::move(p2).SetError(TimeoutError());
    std::move(p1).SetError(TimeoutError());

    ASSERT_TRUE(fail);
  }

  SIMPLE_TEST(BothOk) {
    auto [f1, p1] = futures::Contract<int>();
    auto [f2, p2] = futures::Contract<int>();

    auto both = futures::no_alloc::All(std::move(f1), std::move(f2));

    bool ok = false;

    std::move(both)
        | futures::Map([&ok](auto tuple) {
            auto [x, y] = tuple;
            ASSERT_EQ(x, 2);
            ASSERT_EQ(y, 1);
            ok = true;
            return Unit{};
          })
        | futures::Detach();

    std::move(p2).SetValue(1);
    std::move(p1).SetValue(2);

    ASSERT_TRUE(ok);
  }

  SIMPLE_TEST(BothFailure1) {
    auto [f1, p1] = futures::Contract<int>();
    auto [f2, p2] = futures::Contract<int>();

    auto both = futures::no_alloc::All(std::move(f1), std::move(f2) | futures::AndThen([](int){
      WHEELS_PANIC("Don't run me!");
      return 42;
    }));

    bool fail = false;

    std::move(both)
        | futures::OrElse([&fail](Error e) -> Result<std::tuple<int, int>> {
            ASSERT_EQ(e, TimeoutError());
            fail = true;
            return result::Err(e);
          })
        | futures::Detach();

    std::move(p1).SetError(TimeoutError());

    std::move(p2).SetValue(7);

    ASSERT_TRUE(fail);
  }

  SIMPLE_TEST(BothFailure2) {
    auto [f1, p1] = futures::Contract<int>();
    auto [f2, p2] = futures::Contract<int>();

    auto both = futures::no_alloc::All(std::move(f1) | futures::AndThen([](int){
      WHEELS_PANIC("Don't run me!");
      return 42;
    }), std::move(f2));

    bool fail = false;

    std::move(both)
        | futures::OrElse([&fail](Error e) -> Result<std::tuple<int, int>> {
            ASSERT_EQ(e, IoError());
            fail = true;
            return result::Err(e);
          })
        | futures::Detach();

    std::move(p2).SetError(IoError());

    std::move(p1).SetValue(4);

    ASSERT_TRUE(fail);
  }

  SIMPLE_TEST(BothTypes) {
    auto [f1, p1] = futures::Contract<std::string>();
    auto [f2, p2] = futures::Contract<std::tuple<int, int>>();

    auto both = futures::Both(std::move(f1), std::move(f2));

    bool ok = false;

    std::move(both)
        | futures::Map([&ok](auto tuple) {
            auto [x, y] = tuple;

            ASSERT_EQ(x, "3");

            std::tuple<int, int> t = {1, 2};
            ASSERT_EQ(y, t);

            ok = true;
            return Unit{};
          })
        | futures::Detach();

    std::move(p2).SetValue({1, 2});
    std::move(p1).SetValue("3");

    ASSERT_TRUE(ok);
  }

  SIMPLE_TEST(QuorumOk1){
    auto [f1, p1] = futures::Contract<int>();
    auto [f2, p2] = futures::Contract<int>();
    auto [f3, p3] = futures::Contract<int>();

    auto quorum = futures::no_alloc::Quorum(2, std::move(f1) | futures::AndThen([](int){
      ASSERT_TRUE(false);
      return 5;
    }), std::move(f2), std::move(f3)) | futures::Start();

    std::move(p2).SetValue(42);
    std::move(p3).SetValue(37);
    std::move(p1).SetValue(3);

    auto ans = std::move(quorum) | futures::Get();

    ASSERT_TRUE(ans);

    ASSERT_EQ(ans->size(), 2);

    ASSERT_EQ((*ans)[0], 42);

    ASSERT_EQ((*ans)[1], 37);
  }

  SIMPLE_TEST(QuorumOk2){
    auto [f1, p1] = futures::Contract<int>();
    auto [f2, p2] = futures::Contract<int>();
    auto [f3, p3] = futures::Contract<int>();

    auto quorum = futures::no_alloc::Quorum(2, std::move(f1), std::move(f2), std::move(f3));

    std::move(p1).SetValue(42);
    std::move(p2).SetError(TimeoutError());
    std::move(p3).SetValue(37);

    auto ans = std::move(quorum) | futures::Get();

    ASSERT_TRUE(ans);

    ASSERT_EQ(ans->size(), 2);

    ASSERT_EQ((*ans)[0], 42);

    ASSERT_EQ((*ans)[1], 37);
  }

  SIMPLE_TEST(QuorumOk3){
    auto [f1, p1] = futures::Contract<int>();
    auto [f2, p2] = futures::Contract<int>();
    auto [f3, p3] = futures::Contract<int>();

    auto quorum = futures::no_alloc::Quorum(2, std::move(f1) | futures::OrElse([](Error){
      ASSERT_TRUE(false);
      return 5;
    }), std::move(f2), std::move(f3)) | futures::Start();

    std::move(p2).SetValue(42);
    std::move(p3).SetValue(37);
    std::move(p1).SetError(TimeoutError());

    auto ans = std::move(quorum) | futures::Get();

    ASSERT_TRUE(ans);

    ASSERT_EQ(ans->size(), 2);

    ASSERT_EQ((*ans)[0], 42);

    ASSERT_EQ((*ans)[1], 37);
  }

  SIMPLE_TEST(QuorumFailure1){
    auto [f1, p1] = futures::Contract<int>();
    auto [f2, p2] = futures::Contract<int>();
    auto [f3, p3] = futures::Contract<int>();

    auto quorum = futures::no_alloc::Quorum(2, std::move(f1), std::move(f2), std::move(f3));

    std::move(p1).SetValue(42);
    std::move(p2).SetError(TimeoutError());
    std::move(p3).SetError(IoError());

    auto ans = std::move(quorum) | futures::Get();
    ASSERT_TRUE(!ans);

    ASSERT_EQ(ans.error(), IoError());
  }

  SIMPLE_TEST(QuorumFailure2){
    auto [f1, p1] = futures::Contract<int>();
    auto [f2, p2] = futures::Contract<int>();
    auto [f3, p3] = futures::Contract<int>();
    auto [f4, p4] = futures::Contract<int>();

    auto quorum = futures::no_alloc::Quorum(3, std::move(f1), std::move(f2), std::move(f3), std::move(f4) | futures::AndThen([](int){
      ASSERT_TRUE(false);
      return 5;
    })) | futures::Start();

    std::move(p1).SetValue(42);
    std::move(p2).SetError(TimeoutError());
    std::move(p3).SetError(IoError());
    std::move(p4).SetValue(38);

    auto ans = std::move(quorum) | futures::Get();

    ASSERT_TRUE(!ans);

    ASSERT_EQ(ans.error(), IoError());
  }

  SIMPLE_TEST(QuorumFailure3){
    auto [f1, p1] = futures::Contract<int>();
    auto [f2, p2] = futures::Contract<int>();
    auto [f3, p3] = futures::Contract<int>();
    auto [f4, p4] = futures::Contract<int>();

    auto quorum = futures::no_alloc::Quorum(3, std::move(f1), std::move(f2), std::move(f3), std::move(f4) | futures::AndThen([](int){
      ASSERT_TRUE(false);
      return 5;
    })) | futures::Start();

    std::move(p2).SetError(TimeoutError());
    std::move(p3).SetError(IoError());
    std::move(p4).SetValue(38);
    std::move(p1).SetValue(42);

    auto ans = std::move(quorum) | futures::Get();

    ASSERT_TRUE(!ans);

    ASSERT_EQ(ans.error(), IoError());
  }

  SIMPLE_TEST(VectorQuorumOk1){
    auto [f1, p1] = futures::Contract<int>();
    auto [f2, p2] = futures::Contract<int>();
    auto [f3, p3] = futures::Contract<int>();

    std::vector<decltype(f1)> vec{};
    vec.push_back(std::move(f1));
    vec.push_back(std::move(f2));
    vec.push_back(std::move(f3));

    auto quorum = futures::no_alloc::Quorum(2, std::move(vec));
    std::move(p1).SetValue(42);
    std::move(p2).SetError(TimeoutError());
    std::move(p3).SetValue(37);

    auto ans = std::move(quorum) | futures::Get();

    ASSERT_TRUE(ans);

    ASSERT_EQ(ans->size(), 2);

    ASSERT_EQ((*ans)[0], 42);

    ASSERT_EQ((*ans)[1], 37);
  }

  SIMPLE_TEST(VectorQuorumFailure1){
    auto [f1, p1] = futures::Contract<int>();
    auto [f2, p2] = futures::Contract<int>();
    auto [f3, p3] = futures::Contract<int>();

    std::vector<decltype(f1)> vec{};
    vec.push_back(std::move(f1));
    vec.push_back(std::move(f2));
    vec.push_back(std::move(f3));

    auto quorum = futures::no_alloc::Quorum(2, std::move(vec));
    std::move(p1).SetValue(42);
    std::move(p2).SetError(TimeoutError());
    std::move(p3).SetError(IoError());

    auto ans = std::move(quorum) | futures::Get();
    ASSERT_TRUE(!ans);

    ASSERT_EQ(ans.error(), IoError());
  }

  SIMPLE_TEST(MimicFirstOk1) {
    auto [f1, p1] = futures::Contract<int>();
    auto [f2, p2] = futures::Contract<int>();

    auto first = futures::no_alloc::Select(std::move(f1), std::move(f2));

    bool ok = false;

    std::move(first)
        | futures::Map([&ok](auto variant) {
            switch(variant.index()){
              case 0 : {
                ASSERT_EQ(std::get<0>(variant), 2);
                break;
              }
              case 1 : {
                ASSERT_EQ(std::get<1>(variant), 2);
                break;
              }
            }

            ok = true;
            return Unit{};
          })
        | futures::Detach();

    std::move(p2).SetValue(2);

    std::move(p1).SetValue(1);

    ASSERT_TRUE(ok);
  }

  SIMPLE_TEST(MimicFirstOk2) {
    auto [f1, p1] = futures::Contract<int>();
    auto [f2, p2] = futures::Contract<int>();

    auto first = futures::no_alloc::Select(std::move(f1), std::move(f2));

    bool ok = false;

    std::move(first)
        | futures::Map([&ok](auto variant) {
            switch(variant.index()){
              case 0 : {
                ASSERT_EQ(std::get<0>(variant), 29);
                break;
              }
              case 1 : {
                ASSERT_EQ(std::get<1>(variant), 29);
                break;
              }
            }

            ok = true;
            return Unit{};
          })
        | futures::Detach();

    std::move(p1).SetError(TimeoutError());
    std::move(p2).SetValue(29);

    ASSERT_TRUE(ok);
  }

  SIMPLE_TEST(MimicFirstOk3) {
    auto [f1, p1] = futures::Contract<int>();
    auto [f2, p2] = futures::Contract<int>();

    auto first = futures::no_alloc::Select(std::move(f1), std::move(f2));

    bool ok = false;

    std::move(first)
        | futures::Map([&ok](auto variant) {
            switch(variant.index()){
              case 0 : {
                ASSERT_EQ(std::get<0>(variant), 31);
                break;
              }
              case 1 : {
                ASSERT_EQ(std::get<1>(variant), 31);
                break;
              }
            }

            ok = true;
            return Unit{};
          })
        | futures::Detach();

    std::move(p2).SetError(IoError());
    std::move(p1).SetValue(31);

    ASSERT_TRUE(ok);
  }

  SIMPLE_TEST(MimicFirstFailure) {
    auto [f1, p1] = futures::Contract<int>();
    auto [f2, p2] = futures::Contract<int>();

    auto first = futures::no_alloc::Select(std::move(f1), std::move(f2));

    bool fail = false;

    std::move(first)
        | futures::OrElse([&](Error e) -> Result<std::variant<int, int>> {
            ASSERT_EQ(e, TimeoutError());
            fail = true;
            return result::Err(e);
          })
        | futures::Detach();

    std::move(p2).SetError(TimeoutError());
    std::move(p1).SetError(TimeoutError());

    ASSERT_TRUE(fail);
  }

  SIMPLE_TEST(SelectTypes1){
    auto [f1, p1] = futures::Contract<std::string>();
    auto [f2, p2] = futures::Contract<std::tuple<int, int>>();

    auto both = futures::no_alloc::Select(std::move(f1), std::move(f2));

    bool ok = false;

    std::move(both)
        | futures::Map([&ok](auto variant) {
            ASSERT_EQ(variant.index(), 1);

            std::tuple<int, int> t = {1, 2};
            ASSERT_EQ(std::get<1>(variant), t);

            ok = true;
            return Unit{};
          })
        | futures::Detach();

    std::move(p2).SetValue({1, 2});
    std::move(p1).SetValue("3");

    ASSERT_TRUE(ok);
  }

  SIMPLE_TEST(SelectTypes2){
    auto [f1, p1] = futures::Contract<std::string>();
    auto [f2, p2] = futures::Contract<std::tuple<int, int>>();

    auto both = futures::no_alloc::Select(std::move(f1), std::move(f2));

    bool ok = false;

    std::move(both)
        | futures::Map([&ok](auto variant) {
            ASSERT_EQ(variant.index(), 0);

            ASSERT_EQ(std::get<0>(variant), "3");

            ok = true;
            return Unit{};
          })
        | futures::Detach();

    std::move(p1).SetValue("3");
    std::move(p2).SetValue({1, 2});

    ASSERT_TRUE(ok);
  }
}

TEST_SUITE(Cancel){
  SIMPLE_TEST(FirstShortCircuit){
    auto [f, p] = futures::Contract<int>();
  
    auto deadly_signal = std::move(f) | futures::Map([](int){
      WHEELS_PANIC("deadly_signal : Test failed!");
      return 47;
    });

    futures::no_alloc::First(std::move(deadly_signal), futures::Value(42)) | futures::Detach();

    std::move(p).SetValue(13);
  }

  SIMPLE_TEST(CancelFirst){
    auto f1 = futures::Just() | futures::Map([](Unit){
      WHEELS_PANIC("f1 : Test failed!");
    });

    auto f2 = futures::Just() | futures::Map([](Unit){
      WHEELS_PANIC("f2 : Test failed!");
    });

    futures::no_alloc::First(std::move(f1), std::move(f2)) | futures::AndThen([](Unit){
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

    futures::no_alloc::First(futures::no_alloc::First(std::move(f1), std::move(f2)),  futures::Just()) | futures::Detach();

    ASSERT_EQ(manual.Drain(), 2);
  }

  SIMPLE_TEST(BothShortCircuit){
    auto [f, p] = futures::Contract<int>();

    auto deadly_signal = std::move(f) | futures::Map([](int){
      WHEELS_PANIC("deadly_signal : Test failed!");
    });

    futures::no_alloc::All(std::move(deadly_signal), futures::Failure<int>(TimeoutError())) | futures::Detach();

    std::move(p).SetValue(42);
  }

  SIMPLE_TEST(CancelBoth){
    auto f1 = futures::Just() | futures::Map([](Unit){
      WHEELS_PANIC("f1 : Test failed!");
    });

    auto f2 = futures::Just() | futures::Map([](Unit){
      WHEELS_PANIC("f2 : Test failed!");
    });

    futures::no_alloc::All(std::move(f1), std::move(f2)) | futures::AndThen([](std::tuple<Unit, Unit>){
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

    futures::no_alloc::All(futures::no_alloc::All(std::move(f1), std::move(f2)),  futures::Failure<int>(IoError())) | futures::Detach();

    ASSERT_EQ(manual.Drain(), 2);
  }

  SIMPLE_TEST(QuorumShortCiruit){
    auto f1 = futures::Just();

    auto f2 = futures::Just() | futures::AndThen([](Unit) -> Unit {
      WHEELS_PANIC("f2 : Test failed!");
      return {};
    });

    futures::no_alloc::Quorum(1, std::move(f1), std::move(f2)) | futures::Detach();   
  }

  SIMPLE_TEST(CancelQuorum){
    executors::ManualExecutor manual;

    auto f1 = futures::Submit(manual, []{}) | futures::AndThen([](Unit){
      WHEELS_PANIC("f1 : Test failed!");
    });

    auto f2 = futures::Submit(manual, []{}) | futures::AndThen([](Unit){
      WHEELS_PANIC("f2 : Test failed!");
    }); 

    futures::no_alloc::Quorum(1, std::move(f1), std::move(f2)) | futures::Discard();   
  }
}

RUN_ALL_TESTS()