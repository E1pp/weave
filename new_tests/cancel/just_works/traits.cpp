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

#include <weave/futures/traits/cancel.hpp>

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

TEST_SUITE(CancelTraits){
  SIMPLE_TEST(MakeCancellable){
    //
  }

  SIMPLE_TEST(MonadCancellable){
    //
  }

  SIMPLE_TEST(SideEffectsCancellable){
    //
  }

  SIMPLE_TEST(StartCancellable){
    //
  }

  SIMPLE_TEST(ViaCancellable){
    //
  }

  SIMPLE_TEST(BoxNotCancellable){
    //
  }

  SIMPLE_TEST(FlattenCancellable1){
    //
  }

  SIMPLE_TEST(FlattenCancellable2){
    //
  }

  SIMPLE_TEST(ForkCancellable1){
    auto [f1, f2] = futures::Just() | futures::Fork<2>();

    static_assert(futures::traits::Cancellable<decltype(f1)>);

    static_assert(futures::traits::Cancellable<decltype(f2)>);

    std::apply([](auto... fs){
      futures::All(std::move(fs)...) | futures::ThreadAwait();
    }, std::make_tuple(std::move(f1), std::move(f2)));
  }

  SIMPLE_TEST(ForkCancellable2){
    auto [f1, f2] = futures::Just() | futures::Box() | futures::Fork<2>();

    static_assert(!futures::traits::Cancellable<decltype(f1)>);

    static_assert(!futures::traits::Cancellable<decltype(f2)>);

    std::apply([](auto... fs){
      futures::All(std::move(fs)...) | futures::ThreadAwait();
    }, std::make_tuple(std::move(f1), std::move(f2)));
  }

  SIMPLE_TEST(ParallelCancellable1){
    // 
  }

  SIMPLE_TEST(ParallelCancellable2){
    //
  }
}

#endif

RUN_ALL_TESTS()