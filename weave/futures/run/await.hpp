#pragma once

#include <weave/fibers/core/self.hpp>

#include <weave/fibers/sched/suspend.hpp>

#include <weave/futures/run/thread_await.hpp>

#include <weave/futures/syntax/pipe.hpp>

#include <weave/futures/model/evaluation.hpp>

#include <weave/futures/traits/value_of.hpp>

#include <weave/threads/blocking/event.hpp>

#include <weave/satellite/satellite.hpp>

#include <weave/support/constructor_bases.hpp>

#include <optional>

namespace weave::futures {

namespace pipe {

struct [[nodiscard]] Await {
  template <SomeFuture Future>
  class Waiter final : public support::PinnedBase {
   public:
    using ValueType = typename Future::ValueType;

    explicit Waiter(Future f)
        : eval_(std::move(f).Force(*this)),
          token_(cancel::Never()) {
    }

    Result<ValueType> Await() {
      auto awaiter = [this](fibers::FiberHandle handle) mutable {
        fiber_ = handle;
        token_ = fiber_.CancelToken();

        eval_.Start();

        return fibers::FiberHandle::Invalid();
      };

      fibers::Suspend(awaiter);

      return GetResult();
    }

    // Completable
    void Consume(Output<ValueType> o) noexcept {
      res_.emplace(std::move(o.result));

      fiber_.Schedule(o.context.hint_);
    }

    // CancelSource
    void Cancel(Context) noexcept {
      fiber_.Schedule(executors::SchedulerHint::UpToYou);  // Resumed Await()
                                                           // call will throw
    }

    cancel::Token CancelToken() {
      return token_;
    }

   private:
    Result<ValueType> GetResult() {
      if (res_.has_value() && !token_.CancelRequested()) {
        return std::move(*res_);
      } else {
        // Propagate cancellation
        throw cancel::CancelledException{};
      }
    }

   private:
    EvaluationType<Waiter, Future> eval_;
    cancel::Token token_;
    fibers::FiberHandle fiber_;
    std::optional<Result<ValueType>> res_;
  };

  template <SomeFuture InputFuture>
  Result<traits::ValueOf<InputFuture>> Pipe(InputFuture f) {
    if (fibers::Self() == nullptr) {
      return std::move(f) | futures::ThreadAwait();
    }

    return Waiter(std::move(f)).Await();
  }
};

}  // namespace pipe

inline auto Await() {
  return pipe::Await{};
}

}  // namespace weave::futures
