#pragma once

#include <weave/futures/model/evaluation.hpp>
#include <weave/futures/model/thunk.hpp>

#include <weave/result/make/ok.hpp>

#include <weave/support/constructor_bases.hpp>

namespace weave::futures::thunks {

class [[nodiscard]] Just : support::NonCopyableBase {
 public:
  using ValueType = Unit;

  Just() = default;

  // Movable
  Just(Just&&){};
  Just& operator=(Just&&) = default;

 private:
  template <Consumer<ValueType> Cons>
  class JustEvaluation : support::PinnedBase {
   public:
    JustEvaluation(Just, Cons& cons) {
      cons.Complete(result::Ok());
    }
  };

 public:
  template <Consumer<ValueType> Cons>
  Evaluation<Just, Cons> auto Force(Cons& cons) {
    return JustEvaluation<Cons>(std::move(*this), cons);
  }
};

}  // namespace weave::futures::thunks
