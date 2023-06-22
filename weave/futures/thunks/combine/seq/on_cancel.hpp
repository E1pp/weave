#pragma once

#include <weave/futures/thunks/detail/cancel_base.hpp>

#include <weave/futures/traits/value_of.hpp>

#include <weave/satellite/meta_data.hpp>
#include <weave/satellite/satellite.hpp>

#include <wheels/core/defer.hpp>

#include <optional>
#include <type_traits>

namespace weave::futures::thunks {

template <SomeFuture Future, typename F>
class [[nodiscard]] OnCancel final
    : public IConsumer<typename Future::ValueType>,
      public detail::CancellableBase<Future>,
      public executors::Task {
 public:
  using ValueType = typename Future::ValueType;

  explicit OnCancel(Future ft, F fun)
      : future_(std::move(ft)),
        fun_(std::move(fun)) {
  }

  // Non-copyable
  OnCancel(const OnCancel&) = delete;
  OnCancel& operator=(const OnCancel&) = delete;

  // Movable
  OnCancel(OnCancel&& that)
      : future_(std::move(that.future_)),
        fun_(std::move(that.fun_)),
        consumer_(that.consumer_) {
  }
  OnCancel& operator=(OnCancel&&) = default;

  void Start(IConsumer<ValueType>* consumer) {
    consumer_ = consumer;
    future_.Start(this);
  }

  ~OnCancel() override final = default;

 private:
  // IConsumer

  void Consume(Output<ValueType> out) noexcept override final {
    consumer_->Consume(std::move(out));
  }

  // To ensure that side effect is activated
  // in the correct executor we have to submit ourselves
  void Cancel(Context ctx) noexcept override final {
    context_.emplace(std::move(ctx));

    context_->executor_->Submit(this, executors::SchedulerHint::Next);
  }

  cancel::Token CancelToken() override final {
    return consumer_->CancelToken();
  }

  // Task
  void Run() noexcept override final {
    RunSideEffect();

    consumer_->Cancel(std::move(*context_));
  }

  void RunSideEffect() {
    // We prevent instant cancellation within side effect scope
    satellite::MetaData old =
        satellite::SetContext(context_->executor_, cancel::Never());

    wheels::Defer cleanup([&] {
      satellite::RestoreContext(std::move(old));
    });

    std::move(fun_)();
  }

 private:
  Future future_;
  F fun_;
  std::optional<Context> context_;
  IConsumer<ValueType>* consumer_;
};

}  // namespace weave::futures::thunks