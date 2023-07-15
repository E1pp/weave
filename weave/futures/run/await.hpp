#pragma once

#include <weave/fibers/core/fiber.hpp>

#include <weave/fibers/sched/suspend.hpp>

//#include <weave/futures/run/get.hpp>

#include <weave/futures/old_syntax/pipe.hpp>

#include <weave/futures/old_traits/value_of.hpp>

#include <weave/satellite/satellite.hpp>

#include <optional>

namespace weave::futures {

namespace pipe {

struct [[nodiscard]] Await {
  template <SomeFuture Future>
  class Waiter final : public IConsumer<typename Future::ValueType> {
   public:
    using ValueType = typename Future::ValueType;

    explicit Waiter(Future f)
        : future_(std::move(f)),
          token_(satellite::GetToken()) {
    }

    Result<ValueType> Await() {
      auto awaiter = [this](fibers::FiberHandle handle) mutable {
        fiber_ = handle;

        future_.Start(this);

        return fibers::FiberHandle::Invalid();
      };

      fibers::Suspend(awaiter);

      return GetResult();
    }

   private:
    void Consume(Output<ValueType> o) noexcept override final {
      res_.emplace(std::move(o.result));

      fiber_.Schedule(o.context.hint_);
    }

    void Cancel(Context) noexcept override final {
      fiber_.Schedule(
          executors::SchedulerHint::UpToYou);  // Resumed fiber will throw
    }

    cancel::Token CancelToken() override final {
      return token_;
    }

    Result<ValueType> GetResult() {
      if (res_.has_value() && !token_.CancelRequested()) {
        return std::move(*res_);
      } else {
        // Propagate cancellation
        throw cancel::CancelledException{};
      }
    }

   private:
    Future future_;
    cancel::Token token_;
    fibers::FiberHandle fiber_;
    std::optional<Result<ValueType>> res_;
  };

  template <SomeFuture InputFuture>
  Result<traits::ValueOf<InputFuture>> Pipe(InputFuture f) {
    // Check if we are outside of fiber context
    // if (fibers::Fiber::Self() == nullptr) {
    //   return std::move(f) | futures::Get();
    // }

    return Waiter(std::move(f)).Await();
  }
};

}  // namespace pipe

inline auto Await() {
  return pipe::Await{};
}

}  // namespace weave::futures
