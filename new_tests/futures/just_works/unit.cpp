#include <weave/executors/thread_pool.hpp>
#include <weave/executors/manual.hpp>

#include <weave/futures/make/contract.hpp>
#include <weave/futures/make/value.hpp>
#include <weave/futures/make/failure.hpp>
#include <weave/futures/make/submit.hpp>
#include <weave/futures/make/just.hpp>

#include <weave/futures/combine/seq/map.hpp>
#include <weave/futures/combine/seq/and_then.hpp>
// #include <weave/futures/combine/seq/on_success.hpp>
#include <weave/futures/combine/seq/or_else.hpp>
#include <weave/futures/combine/seq/flatten.hpp>
#include <weave/futures/combine/seq/flat_map.hpp>
// #include <weave/futures/combine/seq/fork.hpp>
#include <weave/futures/combine/seq/via.hpp>
#include <weave/futures/combine/seq/box.hpp>
#include <weave/futures/combine/seq/start.hpp>

// #include <weave/futures/combine/par/all.hpp>
#include <weave/futures/combine/par/first.hpp>
// #include <weave/futures/combine/par/quorum.hpp>
#include <weave/futures/combine/par/select.hpp>

#include <weave/futures/run/get.hpp>
#include <weave/futures/run/detach.hpp>

#include <wheels/test/framework.hpp>
#include <wheels/test/util/cpu_timer.hpp>

#include <string>
#include <thread>
#include <tuple>
#include <chrono>

#if !defined(TWIST_FIBERS)

using namespace weave; // NOLINT

using namespace std::chrono_literals;

inline std::error_code TimeoutError() {
  return std::make_error_code(std::errc::timed_out);
}

std::error_code IoError() {
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

// clang-format off

TEST_SUITE(Futures) {
  SIMPLE_TEST(Value) {
    futures::Future<int> auto f = futures::Value(111);

    auto r = std::move(f) | futures::Get();

    ASSERT_TRUE(r);
    ASSERT_EQ(*r, 111);
  }

  SIMPLE_TEST(Just) {
    futures::Future<Unit> auto f = futures::Just();

    auto r = std::move(f) | futures::Get();
    ASSERT_TRUE(r);
    ASSERT_EQ(*r, Unit{});
  }

  SIMPLE_TEST(Failure) {
    auto timeout = TimeoutError();

    futures::Future<int> auto f = futures::Failure<int>(timeout);

    auto r = std::move(f) | futures::Get();

    ASSERT_FALSE(r);
    ASSERT_EQ(r.error(), timeout);
  }

  SIMPLE_TEST(MapValue) {
    auto f = futures::Value(1)
             | futures::Map([](int v) {
                 return v + 1;
               });

    auto r = std::move(f) | futures::Get();

    ASSERT_TRUE(r);
    ASSERT_EQ(*r, 2);
  }

  SIMPLE_TEST(MapError) {
    auto f = futures::Failure<int>(TimeoutError())
             | futures::Map([](int) {
                 FAIL_TEST("Skip this mapper");
                 return Unit{};
               });

    auto r = std::move(f) | futures::Get();

    ASSERT_FALSE(r);
    ASSERT_EQ(r.error(), TimeoutError());
  }

  SIMPLE_TEST(AndThen) {
    auto f = futures::Value<std::string>("ok")
             | futures::AndThen([](std::string s) {
                 return result::Ok(s + "!");
               })
             | futures::AndThen([](std::string s) {
                 return result::Ok(s + "!");
               });

    auto r = std::move(f) | futures::Get();

    ASSERT_TRUE(r);
    ASSERT_EQ(*r, "ok!!");
  }

  SIMPLE_TEST(OrElse) {
    auto f = futures::Failure<std::string>(IoError())
             | futures::OrElse([](Error e) {
                 ASSERT_EQ(e, IoError());
                 return result::Ok(std::string("fallback"));
               });

    auto r = std::move(f) | futures::Get();

    ASSERT_TRUE(r);
    ASSERT_EQ(*r, "fallback");
  }

  SIMPLE_TEST(SubmitPool) {
    executors::ThreadPool pool{4};
    pool.Start();

    futures::Future<int> auto f = futures::Submit(pool, [] {
      return result::Ok(11);
    });

    auto r = std::move(f) | futures::Get();

    ASSERT_TRUE(r);
    ASSERT_EQ(*r, 11);

    pool.Stop();
  }

  SIMPLE_TEST(SubmitManual) {
    executors::ManualExecutor manual;

    bool done = false;

    auto f = futures::Submit(manual, [&] {
      done = true;
      return result::Ok();
    });

    std::move(f) | futures::Detach();

    ASSERT_FALSE(done);

    manual.Drain();

    ASSERT_TRUE(done);
  }

  SIMPLE_TEST(SubmitPoolError) {
    executors::ThreadPool pool{4};
    pool.Start();

    auto f = futures::Submit(pool, []() -> Result<int> {
      return result::Err(IoError());
    });

    auto r = std::move(f) | futures::Get();

    ASSERT_TRUE(!r);
    ASSERT_EQ(r.error(), IoError());

    pool.Stop();
  }

  SIMPLE_TEST(SubmitPoolWait) {
    executors::ThreadPool pool{4};
    pool.Start();

    auto f = futures::Submit(pool, [] {
      std::this_thread::sleep_for(1s);
      return result::Ok(11);
    });

    wheels::ProcessCPUTimer timer;

    auto r = std::move(f) | futures::Get();

    ASSERT_TRUE(timer.Spent() < 100ms);

    ASSERT_TRUE(r);
    ASSERT_EQ(*r, 11);

    pool.Stop();
  }

  SIMPLE_TEST(Flatten0){
    auto ff = futures::Value(futures::Value(42)) | futures::Flatten();

    auto res = std::move(ff) | futures::Get();

    ASSERT_EQ(*res, 42);
  }

  SIMPLE_TEST(Flatten1) {
    executors::ManualExecutor manual;

    auto ff = futures::Submit(manual, [&manual]() {
      auto f = futures::Submit(manual, [] {
        return result::Ok(7);
      });
      return result::Ok(std::move(f));
    });

    futures::Future<int> auto f = std::move(ff) | futures::Flatten();

    bool ok = false;

    std::move(f) | futures::Map([&ok](int v) -> Unit {
      ASSERT_EQ(v, 7);
      ok = true;
      return {};
    }) | futures::Detach();

    ASSERT_FALSE(ok);

    manual.Drain();

    ASSERT_TRUE(ok);
  }

  SIMPLE_TEST(Flatten2) {
    auto ff = futures::Failure<decltype(futures::Value(42))>(IoError());

    Result<int> r = std::move(ff) | futures::Flatten() | futures::Get();

    ASSERT_FALSE(r);
  }

  SIMPLE_TEST(FlatMap) {
    executors::ManualExecutor manual;

    futures::Future<int> auto f = futures::Submit(manual, [] {
                               return result::Ok(23);
                             })
                             | futures::FlatMap([&](int v) {
                                 return futures::Submit(manual, [v] {
                                   return result::Ok(v + 5);
                                 });
                               })
                             | futures::Map([](int v) {
                                 return v + 1;
                               });

    bool ok = true;

    std::move(f)
        | futures::Map([&](int v) {
            ASSERT_EQ(v, 29);
            ok = true;
            return Unit{};
          })
        | futures::Detach();

    manual.Drain();

    ASSERT_TRUE(ok);
  }

  SIMPLE_TEST(Via) {
    executors::ManualExecutor manual1;
    executors::ManualExecutor manual2;

    size_t steps = 0;

    futures::Just()
        | futures::Via(manual1)
        | futures::Map([&](Unit) {
            ++steps;
            return Unit{};
          })
        | futures::AndThen([&](Unit) -> Status {
            ++steps;
            return result::Ok();
          })
        | futures::Map([](Unit) {
            return Unit{};
          })
        | futures::Via(manual2)
        | futures::Map([&](Unit) {
            ++steps;
            return Unit{};
          })
        | futures::OrElse([&](Error) -> Status {
            FAIL_TEST("Skip this");
            return result::Ok();
          })
        | futures::Map([&](Unit) {
            ++steps;
            return Unit{};
          })
        | futures::Via(manual1)
        | futures::Map([&](Unit) {
            ++steps;
            return Unit{};
          })
        | futures::FlatMap([&](Unit) {
            ++steps;
            return futures::Value(1);
          })
        | futures::Map([&](int v) {
            ASSERT_EQ(v, 1);
            ++steps;
            return Unit{};
          })
        | futures::Detach();

    {
      size_t tasks = manual1.Drain();
      ASSERT_EQ(tasks, 3);
    }

    ASSERT_EQ(steps, 2);

    {
      size_t tasks = manual2.Drain();
      ASSERT_GE(tasks, 2);
    }

    ASSERT_EQ(steps, 4);

    {
      size_t tasks = manual1.Drain();
      ASSERT_GE(tasks, 3);
    }

    ASSERT_EQ(steps, 7);
  }

  SIMPLE_TEST(ContractValue) {
    auto [f, p] = futures::Contract<std::string>();

    std::move(p).SetValue("Hi");
    auto r = std::move(f) | futures::Get();

    ASSERT_TRUE(r.has_value());
    ASSERT_EQ(*r, "Hi");
  }

  SIMPLE_TEST(ContractError) {
    auto [f, p] = futures::Contract<int>();

    auto timeout = TimeoutError();

    std::move(p).SetError(timeout);
    auto r = std::move(f) | futures::Get();

    ASSERT_TRUE(!r);
    ASSERT_EQ(r.error(), timeout);
  }

  SIMPLE_TEST(ContractDetach) {
    {
      auto [f, p] = futures::Contract<int>();

      std::move(f) | futures::Detach();
      std::move(p).SetValue(1);
    }

    {
      auto [f, p] = futures::Contract<int>();

      std::move(p).SetValue(1);
      std::move(f) | futures::Detach();
    }
  }

  SIMPLE_TEST(ContractMoveOnly) {
    auto [f, p] = futures::Contract<MoveOnly>();

    std::move(p).SetValue(MoveOnly{});
    auto r = std::move(f) | futures::Get();

    ASSERT_TRUE(r);
  }

  SIMPLE_TEST(ContractNonDefaultConstructible) {
    auto [f, p] = futures::Contract<NonDefaultConstructible>();

    std::move(p).SetValue({128});
    auto r = std::move(f) | futures::Get();

    ASSERT_TRUE(r);
  }

  SIMPLE_TEST(Pipeline) {
    auto [f, p] = futures::Contract<int>();

    auto g = std::move(f) | futures::Map([](int v) {
               return v + 1;
             }) | futures::Map([](int v) {
               return v + 2;
             }) | futures::OrElse([](Error) {
               FAIL_TEST("Skip this");
               return result::Ok(111);
             }) | futures::AndThen([](int) -> Result<int> {
               return result::Err(TimeoutError());
             }) | futures::AndThen([](int v) {
               FAIL_TEST("Skip this");
               return result::Ok(v + 3);
             }) | futures::Map([](int v) {
               FAIL_TEST("Skip this");
               return v + 4;
             }) | futures::OrElse([](Error) -> Result<int> {
               return 17;
             }) | futures::Map([](int v) {
               return v + 1;
             });

    std::move(p).Set(3);

    auto r = std::move(g) | futures::Get();

    ASSERT_TRUE(r);
    ASSERT_EQ(*r, 18);
  }

  SIMPLE_TEST(ContractMap) {
    auto [f, p] = futures::Contract<int>();

    auto g = std::move(f) | futures::Map([](int v) { return v + 1; });

    std::move(p).SetValue(35);

    auto r = std::move(g) | futures::Get();

    ASSERT_TRUE(r);
    ASSERT_EQ(*r, 36);
  }

  SIMPLE_TEST(DoNotWait1) {
    executors::ThreadPool pool{4};
    pool.Start();

    bool submit = false;

    auto f = futures::Submit(pool,
                             [&] {
                               std::this_thread::sleep_for(1s);
                               submit = true;
                               return result::Ok(11);
                             })
             | futures::Map([](int v) {
                 return v + 1;
               });

    ASSERT_FALSE(submit);

    auto r = std::move(f) | futures::Get();

    ASSERT_TRUE(submit);

    ASSERT_TRUE(r);
    ASSERT_EQ(*r, 12);

    pool.Stop();
  }

  SIMPLE_TEST(DoNotWait2) {
    executors::ThreadPool pool{4};
    pool.Start();

    bool submit = false;

    auto f = futures::Submit(pool,
                             [&] {
                               std::this_thread::sleep_for(1s);
                               submit = true;
                               return result::Ok(31);
                             })
             | futures::FlatMap([&](int v) {
                 return futures::Submit(pool, [v] {
                   return result::Ok(v + 1);
                 });
               });

    ASSERT_FALSE(submit);

    auto r = std::move(f) | futures::Get();

    ASSERT_TRUE(submit);

    ASSERT_TRUE(r);
    ASSERT_EQ(*r, 32);

    pool.Stop();
  }

  SIMPLE_TEST(Start) {
    bool done = false;

    auto f = futures::Just()
              | futures::Map([&](Unit) {
                  done = true;
                  return 7;
                })
              | futures::Start();

    ASSERT_TRUE(done);

    auto r = std::move(f) | futures::Get();

    ASSERT_TRUE(r);
    ASSERT_EQ(*r, 7);
  }

  SIMPLE_TEST(BoxValue) {
    futures::BoxedFuture<int> f = futures::Value(1) | futures::Box();

    auto r = std::move(f) | futures::Get();

    ASSERT_TRUE(r);
    ASSERT_EQ(*r, 1);
  }

  SIMPLE_TEST(BoxPipeline) {
    executors::ManualExecutor manual;

    bool done = false;

    futures::BoxedFuture<Unit> f = futures::Just()
                                   | futures::Via(manual)
                                   | futures::Map([](Unit) { return Unit{}; })
                                   | futures::AndThen([](Unit) { return result::Ok(); })
                                   | futures::Map([&](Unit) {
                                       done = true;
                                       return Unit{};
                                     })
                                   | futures::Box();

    std::move(f) | futures::Detach();

    manual.Drain();

    ASSERT_TRUE(done);
  }

  SIMPLE_TEST(AutoBoxing) {
    {
      static const std::string kHello = "Hello";
      futures::BoxedFuture<std::string> f = futures::Value(kHello);

      std::move(f) | futures::Detach();
    }

    {
      futures::BoxedFuture<int> f = futures::Value(2) | futures::Map([](int v) { return v + 1; });

      auto r = std::move(f) | futures::Get();

      ASSERT_TRUE(r);
      ASSERT_EQ(*r, 3);
    }
  }

  SIMPLE_TEST(FirstOk1) {
    auto [f1, p1] = futures::Contract<int>();
    auto [f2, p2] = futures::Contract<int>();

    auto first = futures::First(std::move(f1), std::move(f2));

    bool ok = false;

    std::move(first)
        | futures::Map([&ok](int v) {
            ASSERT_EQ(v, 2);
            ok = true;
            return Unit{};
          })
        | futures::Detach();

    std::move(p2).SetValue(2);

    ASSERT_TRUE(ok);

    std::move(p1).SetValue(1);
  }

  SIMPLE_TEST(FirstOk2) {
    auto [f1, p1] = futures::Contract<int>();
    auto [f2, p2] = futures::Contract<int>();

    auto first = futures::First(std::move(f1), std::move(f2));

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

    auto first = futures::First(std::move(f1), std::move(f2));

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

    auto first = futures::First(std::move(f1), std::move(f2));

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

//   SIMPLE_TEST(BothOk) {
//     auto [f1, p1] = futures::Contract<int>();
//     auto [f2, p2] = futures::Contract<int>();

//     auto both = futures::Both(std::move(f1), std::move(f2));

//     bool ok = false;

//     std::move(both)
//         | futures::Map([&ok](auto tuple) {
//             auto [x, y] = tuple;
//             ASSERT_EQ(x, 2);
//             ASSERT_EQ(y, 1);
//             ok = true;
//             return Unit{};
//           })
//         | futures::Detach();

//     std::move(p2).SetValue(1);
//     std::move(p1).SetValue(2);

//     ASSERT_TRUE(ok);
//   }

//   SIMPLE_TEST(BothFailure1) {
//     auto [f1, p1] = futures::Contract<int>();
//     auto [f2, p2] = futures::Contract<int>();

//     auto both = futures::Both(std::move(f1), std::move(f2));

//     bool fail = false;

//     std::move(both)
//         | futures::OrElse([&fail](Error e) -> Result<std::tuple<int, int>> {
//             ASSERT_EQ(e, TimeoutError());
//             fail = true;
//             return result::Err(e);
//           })
//         | futures::Detach();

//     std::move(p1).SetError(TimeoutError());

//     ASSERT_TRUE(fail);

//     std::move(p2).SetValue(7);
//   }

//   SIMPLE_TEST(BothFailure2) {
//     auto [f1, p1] = futures::Contract<int>();
//     auto [f2, p2] = futures::Contract<int>();

//     auto both = futures::Both(std::move(f1), std::move(f2));

//     bool fail = false;

//     std::move(both)
//         | futures::OrElse([&fail](Error e) -> Result<std::tuple<int, int>> {
//             ASSERT_EQ(e, IoError());
//             fail = true;
//             return result::Err(e);
//           })
//         | futures::Detach();

//     std::move(p2).SetError(IoError());

//     ASSERT_TRUE(fail);

//     std::move(p1).SetValue(4);
//   }

//   SIMPLE_TEST(BothTypes) {
//     auto [f1, p1] = futures::Contract<std::string>();
//     auto [f2, p2] = futures::Contract<std::tuple<int, int>>();

//     auto both = futures::Both(std::move(f1), std::move(f2));

//     bool ok = false;

//     std::move(both)
//         | futures::Map([&ok](auto tuple) {
//             auto [x, y] = tuple;

//             ASSERT_EQ(x, "3");

//             std::tuple<int, int> t = {1, 2};
//             ASSERT_EQ(y, t);

//             ok = true;
//             return Unit{};
//           })
//         | futures::Detach();

//     std::move(p2).SetValue({1, 2});
//     std::move(p1).SetValue("3");

//     ASSERT_TRUE(ok);
//   }

//   SIMPLE_TEST(QuorumOk1){
//     auto [f1, p1] = futures::Contract<int>();
//     auto [f2, p2] = futures::Contract<int>();

//     bool flag = false;

//     auto quorum = futures::Quorum(1, std::move(f1), std::move(f2) | futures::AndThen([](int){
//       ASSERT_TRUE(false);
//       return 5;
//     }));

//     std::move(quorum) | futures::AndThen([&](auto vector){
//       ASSERT_EQ(vector.size(), 1);
//       ASSERT_EQ(vector[0], 42);
//       flag = true;
//     }) | futures::Detach();

//     std::move(p1).SetValue(42);

//     ASSERT_TRUE(flag);

//     std::move(p2).SetValue(47);
//   }

//   SIMPLE_TEST(QuorumOk2){
//     auto [f1, p1] = futures::Contract<int>();
//     auto [f2, p2] = futures::Contract<int>();
//     auto [f3, p3] = futures::Contract<int>();

//     auto quorum = futures::Quorum(2, std::move(f1) | futures::AndThen([](int){
//       ASSERT_TRUE(false);
//       return 5;
//     }), std::move(f2), std::move(f3));

//     std::move(p2).SetValue(42);
//     std::move(p3).SetValue(37);

//     auto ans = std::move(quorum) | futures::Get();

//     ASSERT_TRUE(ans);

//     ASSERT_EQ(ans->size(), 2);

//     ASSERT_EQ((*ans)[0], 42);

//     ASSERT_EQ((*ans)[1], 37);

//     std::move(p1).SetValue(3);
//   }

//   SIMPLE_TEST(QuorumOk3){
//     auto [f1, p1] = futures::Contract<int>();
//     auto [f2, p2] = futures::Contract<int>();
//     auto [f3, p3] = futures::Contract<int>();

//     auto quorum = futures::Quorum(2, std::move(f1), std::move(f2), std::move(f3));
//     std::move(p1).SetValue(42);
//     std::move(p2).SetError(TimeoutError());
//     std::move(p3).SetValue(37);

//     auto ans = std::move(quorum) | futures::Get();

//     ASSERT_TRUE(ans);

//     ASSERT_EQ(ans->size(), 2);

//     ASSERT_EQ((*ans)[0], 42);

//     ASSERT_EQ((*ans)[1], 37);
//   }

//   SIMPLE_TEST(QuorumOk4){
//     auto [f1, p1] = futures::Contract<int>();
//     auto [f2, p2] = futures::Contract<int>();
//     auto [f3, p3] = futures::Contract<int>();

//     auto quorum = futures::Quorum(2, std::move(f1) | futures::OrElse([](Error){
//       ASSERT_TRUE(false);
//       return 5;
//     }), std::move(f2), std::move(f3));


//     std::move(p2).SetValue(42);
//     std::move(p3).SetValue(37);

//     auto ans = std::move(quorum) | futures::Get();

//     std::move(p1).SetError(TimeoutError());

//     ASSERT_TRUE(ans);

//     ASSERT_EQ(ans->size(), 2);

//     ASSERT_EQ((*ans)[0], 42);

//     ASSERT_EQ((*ans)[1], 37);
//   }

//   SIMPLE_TEST(QuorumFailure1){
//     auto [f1, p1] = futures::Contract<int>();
//     auto [f2, p2] = futures::Contract<int>();
//     auto [f3, p3] = futures::Contract<int>();

//     auto quorum = futures::Quorum(2, std::move(f1), std::move(f2), std::move(f3));
//     std::move(p1).SetValue(42);
//     std::move(p2).SetError(TimeoutError());
//     std::move(p3).SetError(IoError());

//     auto ans = std::move(quorum) | futures::Get();
//     ASSERT_TRUE(!ans);

//     ASSERT_EQ(ans.error(), IoError());
//   }

//   SIMPLE_TEST(QuorumFailure2){
//     auto [f1, p1] = futures::Contract<int>();
//     auto [f2, p2] = futures::Contract<int>();
//     auto [f3, p3] = futures::Contract<int>();
//     auto [f4, p4] = futures::Contract<int>();

//     auto quorum = futures::Quorum(3, std::move(f1), std::move(f2), std::move(f3), std::move(f4) | futures::AndThen([](int){
//       ASSERT_TRUE(false);
//       return 5;
//     }));

//     std::move(p1).SetValue(42);
//     std::move(p2).SetError(TimeoutError());
//     std::move(p3).SetError(IoError());

//     auto ans = std::move(quorum) | futures::Get();

//     std::move(p4).SetValue(38);

//     ASSERT_TRUE(!ans);

//     ASSERT_EQ(ans.error(), IoError());
//   }

//   SIMPLE_TEST(QuorumFailure3){
//     auto [f1, p1] = futures::Contract<int>();
//     auto [f2, p2] = futures::Contract<int>();
//     auto [f3, p3] = futures::Contract<int>();
//     auto [f4, p4] = futures::Contract<int>();

//     auto quorum = futures::Quorum(3, std::move(f1), std::move(f2), std::move(f3), std::move(f4) | futures::AndThen([](int){
//       ASSERT_TRUE(false);
//       return 5;
//     }));

//     std::move(p2).SetError(TimeoutError());
//     std::move(p3).SetError(IoError());
//     std::move(p4).SetValue(38);
//     std::move(p1).SetValue(42);

//     auto ans = std::move(quorum) | futures::Get();

//     ASSERT_TRUE(!ans);

//     ASSERT_EQ(ans.error(), IoError());
//   }

  SIMPLE_TEST(MimicFirstOk1) {
    auto [f1, p1] = futures::Contract<int>();
    auto [f2, p2] = futures::Contract<int>();

    auto first = futures::Select(std::move(f1), std::move(f2));

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

    ASSERT_TRUE(ok);

    std::move(p1).SetValue(1);
  }

  SIMPLE_TEST(MimicFirstOk2) {
    auto [f1, p1] = futures::Contract<int>();
    auto [f2, p2] = futures::Contract<int>();

    auto first = futures::Select(std::move(f1), std::move(f2));

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

    auto first = futures::Select(std::move(f1), std::move(f2));

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

    auto first = futures::Select(std::move(f1), std::move(f2));

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

    auto both = futures::Select(std::move(f1), std::move(f2));

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

    auto both = futures::Select(std::move(f1), std::move(f2));

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

  SIMPLE_TEST(Inline1) {
    executors::ManualExecutor manual;

    bool ok = false;

    futures::Just()
        | futures::Via(manual)
        | futures::Map([&](Unit) {
            ok = true;
            return Unit{};
          })
        | futures::Detach();

    size_t tasks = manual.Drain();
    ASSERT_TRUE(ok);
    ASSERT_EQ(tasks, 1);
  }

  SIMPLE_TEST(Inline2) {
    executors::ManualExecutor manual;

    auto r = futures::Value(1)
             | futures::Via(manual)
             | futures::Get();

    ASSERT_TRUE(r);
    ASSERT_EQ(*r, 1);
  }

  SIMPLE_TEST(Inline3) {
    executors::ManualExecutor manual;

    bool flat_map = false;
    bool map1 = false;
    bool map2 = false;

    futures::Just()
        | futures::Via(manual)
        | futures::FlatMap([&](Unit) {
            flat_map = true;
            return futures::Value(Unit{});
          })
        | futures::Map([&](Unit u) {
            map1 = true;
            return u;
          })
        | futures::Map([&](Unit u) {
            map2 = true;
            return u;
          })
        | futures::Detach();

    ASSERT_TRUE(manual.RunNext());
    ASSERT_TRUE(flat_map);
    ASSERT_FALSE(map1);

    ASSERT_TRUE(manual.RunNext());
    ASSERT_TRUE(map1);
    ASSERT_FALSE(map2);

    ASSERT_EQ(manual.Drain(), 1);
    ASSERT_TRUE(map2);
  }

  SIMPLE_TEST(Inline4) {
    executors::ManualExecutor manual;

    futures::Submit(manual, [&] {
      auto g = futures::Submit(manual, [] {
        return result::Ok(19);
      });
      return result::Ok(std::move(g));
    }) | futures::Flatten() | futures::Detach();

    size_t tasks = manual.Drain();
    ASSERT_EQ(tasks, 2);
  }

  SIMPLE_TEST(Inline5) {
    executors::ManualExecutor manual;

    auto [f1, p1] = futures::Contract<int>();
    auto [f2, p2] = futures::Contract<int>();

    auto first = futures::First(
        std::move(f1) | futures::Via(manual),
        std::move(f2) | futures::Via(manual));

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

    size_t tasks = manual.Drain();
    ASSERT_EQ(tasks, 0);

    ASSERT_TRUE(ok);
  }

  SIMPLE_TEST(VectorFirstOk1) {
    auto [f1, p1] = futures::Contract<int>();
    auto [f2, p2] = futures::Contract<int>();

    std::vector<futures::BoxedFuture<int>> vec{};
    vec.push_back(std::move(f1));
    vec.push_back(std::move(f2));

    auto first = futures::First(std::move(vec));

    bool ok = false;

    std::move(first)
        | futures::Map([&ok](int v) {
            ASSERT_EQ(v, 2);
            ok = true;
            return Unit{};
          })
        | futures::Detach();

    std::move(p2).SetValue(2);

    ASSERT_TRUE(ok);

    std::move(p1).SetValue(1);
  }

  SIMPLE_TEST(VectorFirstOk2) {
    auto [f1, p1] = futures::Contract<int>();
    auto [f2, p2] = futures::Contract<int>();

    std::vector<futures::BoxedFuture<int>> vec{};
    vec.push_back(std::move(f1));
    vec.push_back(std::move(f2));

    auto first = futures::First(std::move(vec));

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

  SIMPLE_TEST(VectorFirstOk3) {
    auto [f1, p1] = futures::Contract<int>();
    auto [f2, p2] = futures::Contract<int>();

    std::vector<futures::BoxedFuture<int>> vec{};
    vec.push_back(std::move(f1));
    vec.push_back(std::move(f2));

    auto first = futures::First(std::move(vec));

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

  SIMPLE_TEST(VectorFirstFailure) {
    auto [f1, p1] = futures::Contract<int>();
    auto [f2, p2] = futures::Contract<int>();

    std::vector<futures::BoxedFuture<int>> vec{};
    vec.push_back(std::move(f1));
    vec.push_back(std::move(f2));

    auto first = futures::First(std::move(vec));

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

//   SIMPLE_TEST(VectorBothOk) {
//     auto [f1, p1] = futures::Contract<int>();
//     auto [f2, p2] = futures::Contract<int>();

//     std::vector<futures::BoxedFuture<int>> vec{};
//     vec.push_back(std::move(f1));
//     vec.push_back(std::move(f2));

//     auto both = futures::All(std::move(vec));

//     bool ok = false;

//     std::move(both)
//         | futures::Map([&ok](auto vector) {
//             int x = vector[0];
//             int y = vector[1];
//             ASSERT_EQ(x, 2);
//             ASSERT_EQ(y, 1);
//             ok = true;
//             return Unit{};
//           })
//         | futures::Detach();

//     std::move(p2).SetValue(1);
//     std::move(p1).SetValue(2);

//     ASSERT_TRUE(ok);
//   }

//   SIMPLE_TEST(VectorBothFailure1) {
//     auto [f1, p1] = futures::Contract<int>();
//     auto [f2, p2] = futures::Contract<int>();

//     std::vector<futures::BoxedFuture<int>> vec{};
//     vec.push_back(std::move(f1));
//     vec.push_back(std::move(f2));

//     auto both = futures::All(std::move(vec));

//     bool fail = false;

//     std::move(both)
//         | futures::OrElse([&fail](Error e) -> Result<std::vector<int>> {
//             ASSERT_EQ(e, TimeoutError());
//             fail = true;
//             return result::Err(e);
//           })
//         | futures::Detach();

//     std::move(p1).SetError(TimeoutError());

//     ASSERT_TRUE(fail);

//     std::move(p2).SetValue(7);
//   }

//   SIMPLE_TEST(VectorBothFailure2) {
//     auto [f1, p1] = futures::Contract<int>();
//     auto [f2, p2] = futures::Contract<int>();

//     std::vector<futures::BoxedFuture<int>> vec{};
//     vec.push_back(std::move(f1));
//     vec.push_back(std::move(f2));

//     auto both = futures::All(std::move(vec));

//     bool fail = false;

//     std::move(both)
//         | futures::OrElse([&fail](Error e) -> Result<std::vector<int>> {
//             ASSERT_EQ(e, IoError());
//             fail = true;
//             return result::Err(e);
//           })
//         | futures::Detach();

//     std::move(p2).SetError(IoError());

//     ASSERT_TRUE(fail);

//     std::move(p1).SetValue(4);
//   }

//   SIMPLE_TEST(VectorQuorumOk1){
//     auto [f1, p1] = futures::Contract<int>();
//     auto [f2, p2] = futures::Contract<int>();

//     bool flag = false;

//     std::vector<futures::BoxedFuture<int>> vec{};
//     vec.push_back(std::move(f1));
//     vec.push_back(std::move(f2) | futures::AndThen([](int){
//       ASSERT_TRUE(false);
//       return 5;
//     }));

//     auto quorum = futures::Quorum(1, std::move(vec));

//     std::move(quorum) | futures::AndThen([&](auto vector){
//       ASSERT_EQ(vector.size(), 1);
//       ASSERT_EQ(vector[0], 42);
//       flag = true;
//     }) | futures::Detach();

//     std::move(p1).SetValue(42);

//     ASSERT_TRUE(flag);

//     std::move(p2).SetValue(47);
//   }

//   SIMPLE_TEST(VectorQuorumOk2){
//     auto [f1, p1] = futures::Contract<int>();
//     auto [f2, p2] = futures::Contract<int>();
//     auto [f3, p3] = futures::Contract<int>();

//     std::vector<futures::BoxedFuture<int>> vec{};
//     vec.push_back(std::move(f1) | futures::AndThen([](int){
//       ASSERT_TRUE(false);
//       return 5;
//     }));
//     vec.push_back(std::move(f2));
//     vec.push_back(std::move(f3));

//     auto quorum = futures::Quorum(2, std::move(vec));

//     std::move(p2).SetValue(42);
//     std::move(p3).SetValue(37);

//     auto ans = std::move(quorum) | futures::Get();

//     ASSERT_TRUE(ans);

//     ASSERT_EQ(ans->size(), 2);

//     ASSERT_EQ((*ans)[0], 42);

//     ASSERT_EQ((*ans)[1], 37);

//     std::move(p1).SetValue(3);
//   }

//   SIMPLE_TEST(VectorQuorumOk3){
//     auto [f1, p1] = futures::Contract<int>();
//     auto [f2, p2] = futures::Contract<int>();
//     auto [f3, p3] = futures::Contract<int>();

//     std::vector<decltype(f1)> vec{};
//     vec.push_back(std::move(f1));
//     vec.push_back(std::move(f2));
//     vec.push_back(std::move(f3));

//     auto quorum = futures::Quorum(2, std::move(vec));
//     std::move(p1).SetValue(42);
//     std::move(p2).SetError(TimeoutError());
//     std::move(p3).SetValue(37);

//     auto ans = std::move(quorum) | futures::Get();

//     ASSERT_TRUE(ans);

//     ASSERT_EQ(ans->size(), 2);

//     ASSERT_EQ((*ans)[0], 42);

//     ASSERT_EQ((*ans)[1], 37);
//   }

//   SIMPLE_TEST(VectorQuorumOk4){
//     auto [f1, p1] = futures::Contract<int>();
//     auto [f2, p2] = futures::Contract<int>();
//     auto [f3, p3] = futures::Contract<int>();

//     std::vector<futures::BoxedFuture<int>> vec{};
//     vec.push_back(std::move(f1) | futures::OrElse([](Error){
//       ASSERT_TRUE(false);
//       return 5;
//     }));
//     vec.push_back(std::move(f2));
//     vec.push_back(std::move(f3));

//     auto quorum = futures::Quorum(2, std::move(vec));

//     std::move(p2).SetValue(42);
//     std::move(p3).SetValue(37);

//     auto ans = std::move(quorum) | futures::Get();

//     std::move(p1).SetError(TimeoutError());

//     ASSERT_TRUE(ans);

//     ASSERT_EQ(ans->size(), 2);

//     ASSERT_EQ((*ans)[0], 42);

//     ASSERT_EQ((*ans)[1], 37);
//   }

//   SIMPLE_TEST(VectorQuorumFailure1){
//     auto [f1, p1] = futures::Contract<int>();
//     auto [f2, p2] = futures::Contract<int>();
//     auto [f3, p3] = futures::Contract<int>();

//     std::vector<decltype(f1)> vec{};
//     vec.push_back(std::move(f1));
//     vec.push_back(std::move(f2));
//     vec.push_back(std::move(f3));

//     auto quorum = futures::Quorum(2, std::move(vec));
//     std::move(p1).SetValue(42);
//     std::move(p2).SetError(TimeoutError());
//     std::move(p3).SetError(IoError());

//     auto ans = std::move(quorum) | futures::Get();
//     ASSERT_TRUE(!ans);

//     ASSERT_EQ(ans.error(), IoError());
//   }

//   SIMPLE_TEST(VectorQuorumFailure2){
//     auto [f1, p1] = futures::Contract<int>();
//     auto [f2, p2] = futures::Contract<int>();
//     auto [f3, p3] = futures::Contract<int>();
//     auto [f4, p4] = futures::Contract<int>();

//     std::vector<futures::BoxedFuture<int>> vec{};
//     vec.push_back(std::move(f1));
//     vec.push_back(std::move(f2));
//     vec.push_back(std::move(f3));
//     vec.push_back(std::move(f4) | futures::AndThen([](int){
//       ASSERT_TRUE(false);
//       return 5;
//     }));

//     auto quorum = futures::Quorum(3, std::move(vec));

//     std::move(p1).SetValue(42);
//     std::move(p2).SetError(TimeoutError());
//     std::move(p3).SetError(IoError());

//     auto ans = std::move(quorum) | futures::Get();

//     std::move(p4).SetValue(38);

//     ASSERT_TRUE(!ans);

//     ASSERT_EQ(ans.error(), IoError());
//   }

//   SIMPLE_TEST(VectorQuorumFailure3){
//     auto [f1, p1] = futures::Contract<int>();
//     auto [f2, p2] = futures::Contract<int>();
//     auto [f3, p3] = futures::Contract<int>();
//     auto [f4, p4] = futures::Contract<int>();

//     std::vector<futures::BoxedFuture<int>> vec{};
//     vec.push_back(std::move(f1));
//     vec.push_back(std::move(f2));
//     vec.push_back(std::move(f3));
//     vec.push_back(std::move(f4) | futures::AndThen([](int){
//       ASSERT_TRUE(false);
//       return 5;
//     }));

//     auto quorum = futures::Quorum(3, std::move(vec));

//     std::move(p2).SetError(TimeoutError());
//     std::move(p3).SetError(IoError());
//     std::move(p4).SetValue(38);
//     std::move(p1).SetValue(42);

//     auto ans = std::move(quorum) | futures::Get();

//     ASSERT_TRUE(!ans);

//     ASSERT_EQ(ans.error(), IoError());
//   }

//   SIMPLE_TEST(ForkOk){
//     auto [f1, f2] = futures::Value(42) | futures::Fork<2>();

//     auto r1 = std::move(f1) | futures::Get();
//     auto r2 = std::move(f2) | futures::Get();

//     ASSERT_TRUE(r1);
//     ASSERT_TRUE(r2);

//     ASSERT_EQ(*r1, 42);
//     ASSERT_EQ(*r2, 42);
//   }

//   SIMPLE_TEST(ForkFailure){
//     auto [f1, f2] = futures::Failure<int>(TimeoutError()) | futures::Fork<2>();

//     auto r1 = std::move(f1) | futures::Get();
//     auto r2 = std::move(f2) | futures::Get();

//     ASSERT_FALSE(r1);
//     ASSERT_FALSE(r2);

//     ASSERT_EQ(r1.error(), TimeoutError());
//     ASSERT_EQ(r2.error(), TimeoutError());   
//   }

//   SIMPLE_TEST(ForkFirst){
//     auto [f1, f2] = futures::Value(42) | futures::Fork<2>();
//     auto res = futures::First(std::move(f1), std::move(f2)) | futures::Get();

//     ASSERT_TRUE(res);
//     ASSERT_EQ(*res, 42);
//   }

//   SIMPLE_TEST(ForkAll){
//     auto [f1, f2] = futures::Value(42) | futures::Fork<2>();

//     auto res = futures::All(std::move(f1), std::move(f2)) | futures::Get();

//     ASSERT_TRUE(res);

//     auto [x, y] = *res;

//     ASSERT_EQ(x, 42);
//     ASSERT_EQ(y, 42);
//   }

//   SIMPLE_TEST(ForkFork){
//     auto [f1, f2] = futures::Value(42) | futures::Fork<2>();
//     auto [f11, f12] = std::move(f1) | futures::Fork<2>();

//     auto all = futures::All(std::move(f11) | futures::Map([](int v){
//       return 2 * v;
//     }), std::move(f12) | futures::Map([](int v){
//       return 3 * v;
//     }), std::move(f2));

//     std::move(all) | futures::Map([](auto tuple){
//       auto [x, y, z] = std::move(tuple);

//       ASSERT_EQ(x, 84);
//       ASSERT_EQ(y, 126);
//       ASSERT_EQ(z, 42);
//     }) | futures::Detach();
//   }

//   SIMPLE_TEST(DontCopySideEffects){
//     int i = 0;
//     auto [f1, f2] = futures::Value(42) | futures::OnSuccess([&]{
//       i++;
//     }) | futures::Fork<2>();

//     futures::Both(std::move(f1), std::move(f2)) | futures::AndThen([&](auto tuple){
//       auto [x, y] = std::move(tuple);

//       ASSERT_EQ(x, 42);
//       ASSERT_EQ(y, 42);
//       ASSERT_EQ(i, 1);
//     }) | futures::Detach();
//   }
}

TEST_SUITE(LazyFutures) {
  SIMPLE_TEST(JustMap) {
    bool run = false;

    auto f = futures::Just() | futures::Map([&](Unit) {
      run = true;
      return Unit{};
    });

    ASSERT_FALSE(run);

    auto r = std::move(f) | futures::Get();
    ASSERT_TRUE(r);
  }

  SIMPLE_TEST(ContractMap) {
    auto [f, p] = futures::Contract<Unit>();

    bool run = false;

    auto g = std::move(f)
             | futures::Map([&](Unit) {
                 run = true;
                 return Unit{};
               });

    std::move(p).SetValue(Unit{});

    ASSERT_FALSE(run);

    std::move(g) | futures::Detach();

    ASSERT_TRUE(run);
  }

  SIMPLE_TEST(Submit) {
    executors::ManualExecutor manual;

    auto f = futures::Submit(manual, [] {
               return result::Ok(7);
             });

    ASSERT_TRUE(manual.IsEmpty());

    std::move(f) | futures::Detach();

    ASSERT_EQ(manual.Drain(), 1);
  }

  SIMPLE_TEST(Monad) {
    executors::ManualExecutor manual;

    auto f = futures::Just()
             | futures::Via(manual)
             | futures::AndThen([](Unit) {
                 return result::Ok(1);
               })
             | futures::Map([](int v) {
                 return v + 1;
               })
             | futures::OrElse([](Error) {
                 return result::Ok(13);
               });

    ASSERT_TRUE(manual.IsEmpty());

    std::move(f) | futures::Detach();

    // ASSERT_EQ
    ASSERT_GE(manual.Drain(), 2);
  }

  SIMPLE_TEST(Flatten) {
    executors::ManualExecutor manual;

    auto f = futures::Value(1)
              | futures::Map([&manual](int v) {
                  return futures::Submit(manual, [v] {
                    return result::Ok(v + 1);
                  });
                })
              | futures::Flatten();

    ASSERT_TRUE(manual.IsEmpty());

    std::move(f) | futures::Detach();

    ASSERT_TRUE(manual.Drain() > 0);
  }

  SIMPLE_TEST(FlatMap) {
    executors::ManualExecutor manual;

    auto f = futures::Just()
             | futures::FlatMap([&manual](Unit) {
                 return futures::Submit(manual, [] {
                   return result::Ok(11);
                 });
               });

    ASSERT_TRUE(manual.IsEmpty());

    std::move(f) | futures::Detach();

    ASSERT_TRUE(manual.Drain() > 0);
  }

  SIMPLE_TEST(Box) {
    executors::ManualExecutor manual;

    futures::BoxedFuture<int> f = futures::Just()
                                  | futures::Via(manual)
                                  | futures::Map([](Unit) {
                                      return 7;
                                    })
                                  | futures::Box();

    ASSERT_TRUE(manual.IsEmpty());

    std::move(f) | futures::Detach();

    ASSERT_TRUE(manual.Drain() > 0);
  }

  SIMPLE_TEST(First) {
    executors::ManualExecutor manual;

    auto f = futures::Just()
             | futures::Via(manual)
             | futures::Map([](Unit) {
                 return 1;
               });

    auto g = futures::Just()
             | futures::Via(manual)
             | futures::Map([](Unit) {
                 return 2;
               });

    auto first = futures::First(std::move(f), std::move(g));

    ASSERT_TRUE(manual.IsEmpty());

    std::move(first) | futures::Detach();

    ASSERT_EQ(manual.Drain(), 2);
  }

  SIMPLE_TEST(Start) {
    executors::ManualExecutor manual;

    auto f = futures::Just()
             | futures::Via(manual)
             | futures::Map([&](Unit) {
                 return 7;
               })
             | futures::Start();

    ASSERT_TRUE(manual.NonEmpty());
    manual.Drain();

    auto r = std::move(f) | futures::Get();

    ASSERT_TRUE(r);
    ASSERT_EQ(*r, 7);
  }

  SIMPLE_TEST(StartMap) {
    executors::ManualExecutor manual;

    auto f = futures::Just()
             | futures::Via(manual)
             | futures::Map([&](Unit) {
                 return 7;
               })
             | futures::Start()
             | futures::Map([](int v) {
                 return v + 1;
               });

    {
      size_t tasks = manual.Drain();
      ASSERT_EQ(tasks, 1);
    }

    std::move(f) | futures::Detach();

    {
      size_t tasks = manual.Drain();
      ASSERT_EQ(tasks, 1);
    }
  }
}

#endif

RUN_ALL_TESTS()
