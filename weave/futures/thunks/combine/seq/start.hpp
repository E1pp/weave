#pragma once

#include <weave/futures/model/evaluation.hpp>

#include <weave/futures/thunks/detail/cancel_base.hpp>

#include <weave/futures/thunks/combine/seq/detail/start_state.hpp>

#include <weave/result/make/err.hpp>

#include <weave/support/constructor_bases.hpp>

namespace weave::futures::thunks {

template <Thunk Future>
class [[nodiscard]] StartFuture final : public support::NonCopyableBase,
                                        public detail::CancellableBase<Future> {
 public:
  using ValueType = typename Future::ValueType;
  using SharedState = detail::SharedState<ValueType>;

  explicit StartFuture(Future fut)
      : state_(new detail::StartState(std::move(fut))) {
  }

  // Movable
  StartFuture(StartFuture&& that) noexcept
      : state_(that.Release()) {
  }
  StartFuture& operator=(StartFuture&&) = delete;

 private:
  template <Consumer<ValueType> Cons>
  class EvaluationFor final : public support::PinnedBase,
                              public AbstractConsumer<ValueType> {
    friend class StartFuture;

    EvaluationFor(StartFuture fut, Cons& cons)
        : state_(fut.Release()),
          cons_(cons) {
    }

   public:
    void Start() {
      std::exchange(state_, nullptr)->Consume(this);
    }

    ~EvaluationFor() override final {
      if(state_ != nullptr){
        std::exchange(state_, nullptr)->Forward(cancel::Signal::Cancel());
      }
    }

   private:
    void Consume(Output<ValueType> o) noexcept override final {
      Complete(cons_, std::move(o));
    }

    void Cancel(Context ctx) noexcept override final {
      cons_.Cancel(std::move(ctx));
    }

    cancel::Token CancelToken() noexcept override final {
      return cons_.CancelToken();
    }

   private:
    SharedState* state_;
    Cons& cons_;
  };

 public:
  template <Consumer<ValueType> Cons>
  Evaluation<StartFuture, Cons> auto Force(Cons& cons) {
    return EvaluationFor<Cons>(std::move(*this), cons);
  }

  void RequestCancel() && {
    Release()->Forward(cancel::Signal::Cancel());
  }

  void ImEager() {
    // No-Op
  }

  ~StartFuture(){
    if(state_ != nullptr){
      Release()->Forward(cancel::Signal::Cancel());
    }
  }

 private:
  SharedState* Release() {
    return std::exchange(state_, nullptr);
  }

 private:
  SharedState* state_;
};

}  // namespace weave::futures::thunks