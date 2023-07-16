#pragma once

// #include <weave/futures/thunks/detail/cancel_base.hpp>

#include <weave/futures/model/evaluation.hpp>

#include <weave/result/make/err.hpp>

#include <weave/satellite/meta_data.hpp>
#include <weave/satellite/satellite.hpp>

#include <weave/support/constructor_bases.hpp>

#include <wheels/core/defer.hpp>

#include <optional>
#include <type_traits>

namespace weave::futures::thunks {

template <Thunk Future, typename F>
class [[nodiscard]] Anyway final: public support::NonCopyableBase {
 public:
  using ValueType = typename Future::ValueType;

  explicit Anyway(Future ft, F fun)
      : future_(std::move(ft)),
        fun_(std::move(fun)) {
  }

  // Movable
  Anyway(Anyway&& that)
      : future_(std::move(that.future_)),
        fun_(std::move(that.fun_)){
  }
  Anyway& operator=(Anyway&&) = default;

 private:
  template <Consumer<ValueType> Cons>
  class EvaluationFor final: public support::PinnedBase, public executors::Task {
   private:
    std::error_code PlaceholderError() {
      return std::make_error_code(static_cast<std::errc>(1337));
    }

   public:
    EvaluationFor(Anyway fut, Cons& cons) : eval_(std::move(fut.future_).Force(*this)), fun_(std::move(fut.fun_)), cons_(cons) {
    }

    void Start(){
      eval_.Start();
    }

    // Completable
    void Consume(Output<ValueType> o) noexcept {
      out_.emplace(std::move(o));

      out_->context.executor_->Submit(this, executors::SchedulerHint::Next);
    }

    // CancelSource
    void Cancel(Context ctx) noexcept {
      out_.emplace(Output<ValueType>({result::Err(PlaceholderError()), std::move(ctx)}));

      out_->context.executor_->Submit(this, executors::SchedulerHint::Next);
    }

    cancel::Token CancelToken() {
      return cons_.CancelToken();
    }

    ~EvaluationFor() override final = default;

   private:
    // Task
    void Run() noexcept override final {
      RunSideEffect();

      if(cons_.CancelToken().CancelRequested()){
        cons_.Cancel(std::move(out_->context));
      } else {
        Complete(cons_, std::move(*out_));
      }
    }

  void RunSideEffect() {
    // We prevent instant cancellation within side effect scope
    satellite::MetaData old =
        satellite::SetContext(out_->context.executor_, cancel::Never());

    wheels::Defer cleanup([&] {
      satellite::RestoreContext(std::move(old));
    });

    std::move(fun_)();
  }

   private:
    EvaluationType<EvaluationFor, Future> eval_;
    F fun_;
    Cons& cons_;
    std::optional<Output<ValueType>> out_{};
  };

 public:
  template<Consumer<ValueType> Cons>
  Evaluation<Anyway, Cons> auto Force(Cons& cons){
    return EvaluationFor<Cons>(std::move(*this), cons);
  }

 private:
  Future future_;
  F fun_;
};

}  // namespace weave::futures::thunks