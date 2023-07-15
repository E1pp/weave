#pragma once

// #include <weave/futures/thunks/detail/cancel_base.hpp>

#include <weave/futures/model/evaluation.hpp>
#include <weave/futures/traits/value_of.hpp>

#include <weave/result/traits/value_of.hpp>

#include <weave/satellite/meta_data.hpp>
#include <weave/satellite/satellite.hpp>

#include <weave/support/constructor_bases.hpp>

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
class [[nodiscard]] Apply : support::NonCopyableBase {
 public:
  using InputValueType = typename Future::ValueType;
  using ValueType = result::traits::ValueOf<typename Mapper::InvokeResult>;

  Apply(Future f, Mapper mapper)
      : future_(std::move(f)),
        mapper_(std::move(mapper)) {
  }

  // Movable
  Apply(Apply&& that)
      : future_(std::move(that.future_)),
        mapper_(std::move(that.mapper_)) {
  }
  Apply& operator=(Apply&&) = default;

 private:
  template <Consumer<ValueType> Cons>
  class ApplyEvaluation final : support::PinnedBase,
                          public executors::Task {
   public:
    ApplyEvaluation(Apply owner, Cons& consumer)
        : map_(std::move(owner.mapper_)),
          consumer_(consumer),
          evaluation_(std::move(owner.future_).Force(*this)) {
    }

    // Satisfies Consumer<InputValueType>
    void Complete(Output<InputValueType> input) noexcept {
      if(CancelToken().CancelRequested()) {
        Cancel(std::move(input.context));
        return;
      }

      if (map_.Predicate(input)) {
        input_.emplace((std::move(input)));
        (*input_).context.executor_->Submit(this, (*input_).context.hint_);

      } else {
        Result<ValueType> forwarded_result = map_.Forward(std::move(input));
        consumer_.Complete({std::move(forwarded_result), input.context});
      }
    }

    void Complete(Result<InputValueType> r) noexcept {
      Complete(Output<InputValueType>({std::move(r), Context{}}));
    }

    void Cancel(Context ctx) noexcept {
      consumer_.Cancel(std::move(ctx));
    }

    cancel::Token CancelToken() {
      return consumer_.CancelToken();
    }

   private:
    void Run() noexcept override final {
    try {
      Result<ValueType> output = RunMapper();

      consumer_.Complete({std::move(output), std::move(input_->context)});
    } catch (cancel::CancelledException) {
      consumer_.Cancel(
          Context{satellite::GetExecutor(), executors::SchedulerHint::UpToYou});
    }
    }

    Result<ValueType> RunMapper() {
      // Setup satellite context
      satellite::MetaData old =
      satellite::SetContext(input_->context.executor_, consumer_.CancelToken());

      // Map() may throw
      wheels::Defer cleanup([old] {
        satellite::RestoreContext(old);
      });

      {
        Mapper moved = std::move(map_);

        return moved.Map(std::move(*input_));
      }
    }

   private:
    Mapper map_;
    Cons& consumer_;
    std::optional<Output<InputValueType>> input_{};
    EvaluationType<ApplyEvaluation, Future> evaluation_;
  };

 public:
  template <Consumer<ValueType> Cons>
  Evaluation<Apply, Cons> auto Force(Cons& cons) {
    return ApplyEvaluation<Cons>(std::move(*this), cons);
  }

 private:
  Future future_;
  Mapper mapper_;
};

}  // namespace weave::futures::thunks