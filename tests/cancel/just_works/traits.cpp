#include <weave/executors/manual.hpp>
#include <weave/timers/processors/standalone.hpp>

// Make 

#include <weave/futures/make/after.hpp>
#include <weave/futures/make/contract.hpp>
#include <weave/futures/make/failure.hpp>
#include <weave/futures/make/just.hpp>
#include <weave/futures/make/never.hpp>
#include <weave/futures/make/submit.hpp>
#include <weave/futures/make/value.hpp>

// Combine Seq

#include <weave/futures/combine/seq/and_then.hpp>
#include <weave/futures/combine/seq/anyway.hpp>
#include <weave/futures/combine/seq/box.hpp>
#include <weave/futures/combine/seq/flatten.hpp>
#include <weave/futures/combine/seq/flat_map.hpp>
#include <weave/futures/combine/seq/fork.hpp>
#include <weave/futures/combine/seq/map.hpp>
#include <weave/futures/combine/seq/on_cancel.hpp>
#include <weave/futures/combine/seq/on_success.hpp>
#include <weave/futures/combine/seq/or_else.hpp>
#include <weave/futures/combine/seq/start.hpp>
#include <weave/futures/combine/seq/via.hpp>
#include <weave/futures/combine/seq/with_timeout.hpp>

// Combine Par

#include <weave/futures/combine/par/all.hpp>
#include <weave/futures/combine/par/first.hpp>
#include <weave/futures/combine/par/quorum.hpp>
#include <weave/futures/combine/par/select.hpp>

// Run

#include <weave/futures/run/await.hpp>
#include <weave/futures/run/discard.hpp>
#include <weave/futures/run/detach.hpp>
#include <weave/futures/run/thread_await.hpp>

// Trait

#include <weave/futures/traits/cancel.hpp>

#include <wheels/test/framework.hpp>

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

#define CANCELLABLE(x) static_assert(futures::traits::Cancellable<decltype(x)>)
#define NOT_CANCELLABLE(x) static_assert(!futures::traits::Cancellable<decltype(x)>)

TEST_SUITE(CancelTraits){
  SIMPLE_TEST(MakeCancellable){
    timers::StandaloneProcessor proc{};
    proc.MakeGlobal();

    CANCELLABLE(futures::After(5s));

    auto [f, p] = futures::Contract<int>();
    std::move(p).SetValue(42);
    std::move(f).RequestCancel();

    CANCELLABLE(f);

    CANCELLABLE(futures::Failure<int>(TimeoutError()));

    CANCELLABLE(futures::Just());

    CANCELLABLE(futures::Never());



    CANCELLABLE(futures::Value(42));
  }

  SIMPLE_TEST(MonadCancellable){
    auto f = futures::Value(42);

    CANCELLABLE(std::move(f) | futures::AndThen([]{}));

    CANCELLABLE(std::move(f) | futures::OrElse([]{
        return 42;
    }));

    CANCELLABLE(std::move(f) | futures::Map([]{}));   
  }

  SIMPLE_TEST(ViaCancellable){
    executors::ManualExecutor manual;

    CANCELLABLE(futures::Just() | futures::Via(manual));

    CANCELLABLE(futures::Submit(manual, []{}));    
  }

  SIMPLE_TEST(SideEffectsCancellable){
    CANCELLABLE(futures::Value(42) | futures::OnCancel([]{}));

    CANCELLABLE(futures::Value(42) | futures::OnSuccess([]{}));

    CANCELLABLE(futures::Just() | futures::Anyway([]{}));
  }

  SIMPLE_TEST(StartCancellable){
    CANCELLABLE(futures::Value(42) | futures::Start());

    CANCELLABLE(futures::Value(42) | futures::Start() | futures::Start());
  }

  SIMPLE_TEST(BoxNotCancellable){
    NOT_CANCELLABLE(futures::Value(42) | futures::Box());

    NOT_CANCELLABLE(futures::Value(42) | futures::Box() | futures::AndThen([]{}));

    NOT_CANCELLABLE(futures::Value(42) | futures::Box() | futures::OrElse([]{
      return 42;
    }));

    NOT_CANCELLABLE(futures::Value(42) | futures::Box() | futures::Map([]{}));

    executors::ManualExecutor manual;

    NOT_CANCELLABLE(futures::Value(42) | futures::Box() | futures::Via(manual));

    NOT_CANCELLABLE(futures::Value(42) | futures::Box() | futures::OnCancel([]{}));

    NOT_CANCELLABLE(futures::Value(42) | futures::Box() | futures::OnSuccess([]{}));

    NOT_CANCELLABLE(futures::Just() | futures::Box() | futures::Anyway([]{}));

    NOT_CANCELLABLE(futures::Value(42) | futures::Box() | futures::Start());

    NOT_CANCELLABLE(futures::Value(42) | futures::Box() | futures::Start() | futures::Start());  
  }

  SIMPLE_TEST(CBoxCancellable){
    CANCELLABLE(futures::Value(42) | futures::CBox());

    CANCELLABLE(futures::Value(42) | futures::CBox() | futures::AndThen([]{}));

    CANCELLABLE(futures::Value(42) | futures::CBox() | futures::OrElse([]{
      return 42;
    }));

    CANCELLABLE(futures::Value(42) | futures::CBox() | futures::Map([]{}));

    executors::ManualExecutor manual;

    CANCELLABLE(futures::Value(42) | futures::CBox() | futures::Via(manual));

    CANCELLABLE(futures::Value(42) | futures::CBox() | futures::OnCancel([]{}));

    CANCELLABLE(futures::Value(42) | futures::CBox() | futures::OnSuccess([]{}));

    CANCELLABLE(futures::Just() | futures::CBox() | futures::Anyway([]{}));

    CANCELLABLE(futures::Value(42) | futures::CBox() | futures::Start());

    CANCELLABLE(futures::Value(42) | futures::CBox() | futures::Start() | futures::Start());    
  }

  SIMPLE_TEST(ForkCancellable1){
    auto [f1, f2] = futures::Just() | futures::Fork<2>();

    CANCELLABLE(f1);
    CANCELLABLE(f2);

    std::apply([](auto... fs){
      futures::All(std::move(fs)...) | futures::ThreadAwait();
    }, std::make_tuple(std::move(f1), std::move(f2)));
  }

  SIMPLE_TEST(ForkCancellable2){
    auto [f1, f2] = futures::Just() | futures::Box() | futures::Fork<2>();

    NOT_CANCELLABLE(f1);
    NOT_CANCELLABLE(f2);

    std::apply([](auto... fs){
      futures::All(std::move(fs)...) | futures::ThreadAwait();
    }, std::make_tuple(std::move(f1), std::move(f2)));
  }

  SIMPLE_TEST(FlattenCancellable1){
    CANCELLABLE(futures::Value(futures::Value(42)) | futures::Flatten());

    CANCELLABLE(futures::Value(42) | futures::FlatMap([]{
      return futures::Value(37);
    }));

    NOT_CANCELLABLE(futures::Value(futures::Value(42)) | futures::Box() |  futures::Flatten());

    NOT_CANCELLABLE(futures::Value(42) | futures::Box() | futures::FlatMap([]{
      return futures::Value(37);
    }));
  }

  SIMPLE_TEST(FlattenCancellable2){
    NOT_CANCELLABLE(futures::Value(futures::Value(42) | futures::Box())|  futures::Flatten());

    NOT_CANCELLABLE(futures::Value(42) | futures::FlatMap([]{
      return futures::Value(37) | futures::Box();
    }));    
  }

  SIMPLE_TEST(ParallelCancellable1){
    CANCELLABLE(futures::First(futures::Value(42), futures::Value(42)));
    CANCELLABLE(futures::First(std::declval<std::vector<decltype(futures::Value(42))>>()));

    CANCELLABLE(futures::Select(futures::Value(42), futures::Value(42)));

    CANCELLABLE(futures::All(futures::Value(42), futures::Value(42)));
    CANCELLABLE(futures::All(std::declval<std::vector<decltype(futures::Value(42))>>()));

    CANCELLABLE(futures::Quorum(1, futures::Value(42), futures::Value(42)));
    CANCELLABLE(futures::Quorum(1, std::declval<std::vector<decltype(futures::Value(42))>>()));
  }

  SIMPLE_TEST(ParallelCancellable2){
    NOT_CANCELLABLE(futures::First(futures::Value(42), futures::Value(42) | futures::Box()));
    NOT_CANCELLABLE(futures::First(std::declval<std::vector<decltype(futures::Value(42) | futures::Box())>>()));

    NOT_CANCELLABLE(futures::All(futures::Value(42), futures::Value(42) | futures::Box()));
    NOT_CANCELLABLE(futures::All(std::declval<std::vector<decltype(futures::Value(42) | futures::Box())>>()));

    NOT_CANCELLABLE(futures::Quorum(1, futures::Value(42), futures::Value(42) | futures::Box()));
    NOT_CANCELLABLE(futures::Quorum(1, std::declval<std::vector<decltype(futures::Value(42) | futures::Box())>>()));

    NOT_CANCELLABLE(futures::Select(futures::Value(42), futures::Value(42) | futures::Box()));
  }

  SIMPLE_TEST(WithTimeoutCancellable){
    CANCELLABLE(futures::Never() | futures::WithTimeout(5s));

    NOT_CANCELLABLE(futures::Never() | futures::Box() | futures::WithTimeout(5s));
  }
}

#endif

RUN_ALL_TESTS()