#pragma once

#include <weave/futures/thunks/detail/cancel_base.hpp>

#include <weave/futures/old_traits/value_of.hpp>

#include <weave/result/traits/value_of.hpp>

#include <weave/satellite/meta_data.hpp>
#include <weave/satellite/satellite.hpp>

#include <wheels/core/defer.hpp>

#include <optional>

#include <type_traits>

namespace weave::futures::thunks {

////////////////////////////////////////////////////////////////////////////////

template <typename T, typename InputFuture>
concept Mapper = SomeFuture<InputFuture> &&
    requires(T mapper, Output<typename InputFuture::ValueType>& out) {
  typename T::InvokeResult;

  { mapper.Predicate(out) } -> std::same_as<bool>;
  { mapper.Map(std::move(out)) } -> std::same_as<typename T::InvokeResult>;
  { mapper.Forward(std::move(out)) } -> std::same_as<typename T::InvokeResult>;
};

////////////////////////////////////////////////////////////////////////////////

template <SomeFuture Future, Mapper<Future> Mapper>
class [[nodiscard]] Apply final : public IConsumer<typename Future::ValueType>,
                                  public executors::Task,
                                  public detail::CancellableBase<Future> {
 public:
  using InputValueType = typename Future::ValueType;
  using ValueType = result::traits::ValueOf<typename Mapper::InvokeResult>;

  Apply(Future f, Mapper mapper)
      : future_(std::move(f)),
        mapper_(std::move(mapper)) {
  }

  // Non-copyable
  Apply(const Apply&) = delete;
  Apply& operator=(const Apply&) = delete;

  // Movable
  Apply(Apply&& that)
      : consumer_(that.consumer_),
        future_(std::move(that.future_)),
        mapper_(std::move(that.mapper_)),
        input_(std::move(that.input_)) {
  }
  Apply& operator=(Apply&&) = default;

  void Start(IConsumer<ValueType>* consumer) {
    consumer_ = consumer;
    future_.Start(this);
  }

 private:
  void Consume(Output<InputValueType> input) noexcept override final {
    if (CancelToken().CancelRequested()) {
      Cancel(std::move(input.context));
      return;
    }

    if (mapper_.Predicate(input)) {
      input_.emplace((std::move(input)));
      (*input_).context.executor_->Submit(this, (*input_).context.hint_);

    } else {
      Result<ValueType> forwarded_result = mapper_.Forward(std::move(input));
      consumer_->Complete({std::move(forwarded_result), input.context});
    }
  }

  void Cancel(Context ctx) noexcept override final {
    consumer_->Cancel(std::move(ctx));
  }

  cancel::Token CancelToken() override final {
    return consumer_->CancelToken();
  }

  void Run() noexcept override final {
    try {
      Result<ValueType> output = RunMapper();

      consumer_->Complete({std::move(output), std::move(input_->context)});
    } catch (cancel::CancelledException) {
      consumer_->Cancel(
          Context{satellite::GetExecutor(), executors::SchedulerHint::UpToYou});
    }
  }

  Result<ValueType> RunMapper() {
    // Setup satellite context
    satellite::MetaData old = satellite::SetContext(input_->context.executor_,
                                                    consumer_->CancelToken());

    // Map() may throw
    wheels::Defer cleanup([old] {
      satellite::RestoreContext(old);
    });

    {
      Mapper moved = std::move(mapper_);

      return moved.Map(std::move(*input_));
    }
  }

 private:
  IConsumer<ValueType>* consumer_;
  Future future_;
  Mapper mapper_;
  std::optional<Output<InputValueType>> input_;
};

}  // namespace weave::futures::thunks