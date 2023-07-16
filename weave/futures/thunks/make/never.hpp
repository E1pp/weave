#pragma once

#include <weave/futures/model/evaluation.hpp>

#include <weave/result/types/unit.hpp>

#include <weave/support/constructor_bases.hpp>

namespace weave::futures::thunks {
//public cancel::SignalReceiver
class [[nodiscard]] Never final : support::NonCopyableBase {
 public:
  using ValueType = Unit;

  Never() = default;

  // Movable
  Never(Never&&) noexcept {};
  Never& operator=(Never&&) = default;

 private:
  template<Consumer<Unit> Cons>
  class EvaluationFor final: public support::PinnedBase, public cancel::SignalReceiver {
   public:
    EvaluationFor(Never, Cons& cons) : cons_(cons) {
    }

    void Start(){
      cons_.CancelToken().Attach(this);
    }

   private:
    void Forward(cancel::Signal signal) override final {
      if (signal.CancelRequested()) {
        cons_.Cancel(Context{});

        return;
      }

      WHEELS_PANIC("You must not release futures::Never!");
    }

   private:
    Cons& cons_;
  };

 public:
  template<Consumer<Unit> Cons>
  Evaluation<Never, Cons> auto Force(Cons& cons){
    return EvaluationFor<Cons>(std::move(*this), cons);
  }
};

}  // namespace weave::futures::thunks