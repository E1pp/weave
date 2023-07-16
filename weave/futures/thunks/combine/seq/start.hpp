#pragma once

// #include <weave/futures/thunks/detail/cancel_base.hpp>

#include <weave/futures/model/evaluation.hpp>

#include <weave/futures/thunks/combine/seq/detail/start_state.hpp>

#include <weave/result/make/err.hpp>

#include <weave/support/constructor_bases.hpp>

namespace weave::futures::thunks {

template <Thunk Future>
class [[nodiscard]] StartFuture final : public support::NonCopyableBase {
 public:
  using ValueType = typename Future::ValueType;
  using SharedState = detail::SharedState<ValueType>; // Not Implemented

  explicit StartFuture(Future fut) : state_(new detail::StartState(std::move(fut))) {
  }

  // Movable
  StartFuture(StartFuture&& that) noexcept: state_(that.Release()){
  }
  StartFuture& operator=(StartFuture&&) = delete;

 private:
  template <Consumer<ValueType> Cons>
  class StartEvaluation final : public support::PinnedBase,
                                public AbstractConsumer<ValueType> {
   public:
    StartEvaluation(StartFuture fut, Cons& cons) : state_(fut.Release()), cons_(cons) {
    }

    void Start(){
      std::exchange(state_, nullptr)->Consume(this);
    }

    ~StartEvaluation() override final {
      WHEELS_VERIFY(state_ == nullptr, "Must use Evaluation!");
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
  Evaluation<StartFuture, Cons> auto Force(Cons& cons){
    return StartEvaluation<Cons>(std::move(*this), cons);
  }

  void RequestCancel() && {
    Release()->Forward(cancel::Signal::Cancel());
  }

  ~StartFuture(){
    WHEELS_VERIFY(state_ == nullptr, "Unfulfilled future!");
  }

 private:
  SharedState* Release(){
    return std::exchange(state_, nullptr);
  }

 private:
  SharedState* state_;
};

} // namespace weave::futures::thunks