#pragma once

#include <weave/futures/model/evaluation.hpp>

#include <weave/result/make/ok.hpp>

#include <weave/support/constructor_bases.hpp>

namespace weave::futures::thunks {

class [[nodiscard]] Just final : public support::NonCopyableBase {
 public:
  using ValueType = Unit;

  Just() = default;

  // Movable
  Just(Just&&) noexcept {};
  Just& operator=(Just&&) = default;

 private:
  template <Consumer<ValueType> Cons>
  class JustEvaluation final : public support::PinnedBase {
   public:
    JustEvaluation(Just, Cons& consumer) : consumer_(consumer) {
    }

    void Start(){
      if(consumer_.CancelToken().CancelRequested()){
        consumer_.Cancel(Context{});
      } else {
        Complete(consumer_, result::Ok());
      }
    }

   private:
    Cons& consumer_;
  };

 public:
  template <Consumer<ValueType> Cons>
  Evaluation<Just, Cons> auto Force(Cons& cons) {
    return JustEvaluation<Cons>(std::move(*this), cons);
  }
};

}  // namespace weave::futures::thunks
