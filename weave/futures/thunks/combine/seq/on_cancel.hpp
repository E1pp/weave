#pragma once

#include <weave/futures/model/evaluation.hpp>

#include <weave/futures/thunks/detail/cancel_base.hpp>

#include <weave/satellite/meta_data.hpp>
#include <weave/satellite/satellite.hpp>

#include <weave/support/constructor_bases.hpp>

#include <wheels/core/defer.hpp>

#include <optional>
#include <type_traits>

namespace weave::futures::thunks {

template <Thunk Future, typename F>
class [[nodiscard]] OnCancel final : public support::NonCopyableBase, public detail::CancellableBase<Future> {
 public:
  using ValueType = typename Future::ValueType;

  explicit OnCancel(Future ft, F fun)
      : future_(std::move(ft)),
        fun_(std::move(fun)) {
  }

  // Movable
  OnCancel(OnCancel&& that) noexcept
      : future_(std::move(that.future_)),
        fun_(std::move(that.fun_)) {
  }
  OnCancel& operator=(OnCancel&&) = default;

 private:
  template <Consumer<ValueType> Cons>
  class EvaluationFor final : public support::PinnedBase,
                              public executors::Task {
   public:
    EvaluationFor(OnCancel fut, Cons& cons)
        : eval_(std::move(fut.future_).Force(*this)),
          fun_(std::move(fut.fun_)),
          cons_(cons) {
    }

    void Start() {
      eval_.Start();
    }

    // Completable
    void Consume(Output<ValueType> o) noexcept {
      Complete(cons_, std::move(o));
    }

    // CancelSource
    // To ensure that side effect is activated
    // in the correct executor we have to submit ourselves
    void Cancel(Context ctx) noexcept {
      ctx_.emplace(std::move(ctx));

      ctx_->executor_->Submit(this, executors::SchedulerHint::Next);
    }

    cancel::Token CancelToken() {
      return cons_.CancelToken();
    }

    ~EvaluationFor() override final = default;

   private:
    // Task
    void Run() noexcept override final {
      RunSideEffect();

      cons_.Cancel(std::move(*ctx_));
    }

    void RunSideEffect() {
      // We prevent instant cancellation within side effect scope
      satellite::MetaData old =
          satellite::SetContext(ctx_->executor_, cancel::Never());

      wheels::Defer cleanup([&] {
        satellite::RestoreContext(std::move(old));
      });

      std::move(fun_)();
    }

   private:
    EvaluationType<EvaluationFor, Future> eval_;
    F fun_;
    Cons& cons_;
    std::optional<Context> ctx_{};
  };

 public:
  template <Consumer<ValueType> Cons>
  Evaluation<OnCancel, Cons> auto Force(Cons& cons) {
    return EvaluationFor<Cons>(std::move(*this), cons);
  }

 private:
  Future future_;
  F fun_;
};

}  // namespace weave::futures::thunks