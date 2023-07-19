#pragma once

#include <weave/futures/model/evaluation.hpp>

#include <weave/futures/thunks/detail/shared_state.hpp>

#include <weave/support/constructor_bases.hpp>

namespace weave::futures::thunks {

// Always Cancellable
template <typename T>
class [[nodiscard]] ContractFuture final : public support::NonCopyableBase {
 public:
  using ValueType = T;
  using SharedState = detail::SharedState<T>;

  explicit ContractFuture(SharedState* state)
      : state_(state) {
  }

  // Movable
  ContractFuture(ContractFuture&& that) noexcept
      : state_(that.Release()) {
  }
  ContractFuture& operator=(ContractFuture&&) = delete;

 private:
  template <Consumer<ValueType> Cons>
  class EvaluationFor final : public support::PinnedBase,
                              public AbstractConsumer<ValueType> {
    friend class ContractFuture;

    using U = ValueType;

    EvaluationFor(ContractFuture fut, Cons& cons)
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
    // AbstractConsumer<ValueType>
    void Consume(Output<U> o) noexcept override final {
      Complete(cons_, std::move(o));
    }

    void Cancel(Context ctx) noexcept override final {
      cons_.Cancel(std::move(ctx));
    }

    cancel::Token CancelToken() override final {
      return cons_.CancelToken();
    }

   private:
    SharedState* state_;
    Cons& cons_;
  };

 public:
  template <Consumer<ValueType> Cons>
  Evaluation<ContractFuture, Cons> auto Force(Cons& cons) {
    return EvaluationFor<Cons>(std::move(*this), cons);
  }

  void RequestCancel() && {
    Release()->Forward(cancel::Signal::Cancel());
  }

  void ImEager() {
    // No-Op
  }

  void Cancellable() {
    // No-Op
  }

  ~ContractFuture(){
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