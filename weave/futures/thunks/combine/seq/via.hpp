#pragma once

#include <weave/executors/executor.hpp>

#include <weave/futures/model/evaluation.hpp>

#include <weave/futures/types/future.hpp>

#include <weave/support/constructor_bases.hpp>

// #include <weave/futures/thunks/detail/cancel_base.hpp>

namespace weave::futures::thunks {

// Via is seemless thus no need for lookup of CancelRequested inside to be
// Cancellable
template <SomeFuture Future>
class [[nodiscard]] Via : support::NonCopyableBase {
 public:
  using ValueType = typename Future::ValueType;

  Via(Future f, Context context)
      : future_(std::move(f)),
        next_context_(context) {
  }

  // Movable
  Via(Via&& that) : future_(std::move(that.future_)),
                    next_context_(std::move(that.next_context_)){
                    } 
  Via& operator=(Via&& that) = default;

 private:
  template <Consumer<ValueType> Cons>
  class ViaEvaluation : support::PinnedBase {
   public:
    ViaEvaluation(Via owner, Cons& consumer) : next_context_(std::move(owner.next_context_)),
                                               consumer_(consumer),
                                               eval_(std::move(owner.future_).Force(*this)){
                                               }

    // Consumer<ValueType>
    void Complete(Result<ValueType> r){
      Complete({std::move(r), Context{}});
    }

    void Complete(Output<ValueType> o){
      o.context = std::move(next_context_);
      consumer_.Complete(std::move(o));
    }

    // insert your own context
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
    EvaluationType<ViaEvaluation, Future> eval_;
  };


 public:
  template <Consumer<ValueType> Cons>
  Evaluation<Via, Cons> auto Force(Cons& cons){
    return ViaEvaluation<Cons>(std::move(*this), cons);
  }

 private:
  Future future_;
  Context next_context_;
};

}  // namespace weave::futures::thunks