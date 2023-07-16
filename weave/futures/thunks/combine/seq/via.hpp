#pragma once

#include <weave/executors/executor.hpp>

#include <weave/futures/model/evaluation.hpp>

#include <weave/futures/types/future.hpp>

#include <weave/support/constructor_bases.hpp>

// #include <weave/futures/thunks/detail/cancel_base.hpp>

namespace weave::futures::thunks {

// Via is seemless thus no need for lookup of CancelRequested inside to be
// Cancellable
template <Thunk Future>
class [[nodiscard]] Via final : public support::NonCopyableBase {
 public:
  using ValueType = typename Future::ValueType;

  Via(Future f, Context context)
      : future_(std::move(f)),
        next_context_(context) {
  }

  // Movable
  Via(Via&& that) noexcept: future_(std::move(that.future_)), next_context_(std::move(that.next_context_)){
                    } 
  Via& operator=(Via&& that) = default;

 private:
  template <Consumer<ValueType> Cons>
  class EvaluationFor final: public support::PinnedBase {
   public:
    EvaluationFor(Via fut, Cons& consumer) noexcept: next_context_(std::move(fut.next_context_)), consumer_(consumer), eval_(std::move(fut.future_).Force(*this)){
    }

    void Start() {
      eval_.Start();
    }

    // Completable<ValueType>
    void Consume(Output<ValueType> o) noexcept {
      o.context = std::move(next_context_);
      Complete(consumer_, std::move(o));
    }

    // CancelSource
    void Cancel(Context) noexcept {
      consumer_.Cancel(std::move(next_context_));
    }

    cancel::Token CancelToken() {
      // forward down the chain
      return consumer_.CancelToken();
    }

   private:
    Context next_context_;
    Cons& consumer_;
    EvaluationType<EvaluationFor, Future> eval_;
  };


 public:
  template <Consumer<ValueType> Cons>
  Evaluation<Via, Cons> auto Force(Cons& cons){
    return EvaluationFor<Cons>(std::move(*this), cons);
  }

 private:
  Future future_;
  Context next_context_;
};

}  // namespace weave::futures::thunks