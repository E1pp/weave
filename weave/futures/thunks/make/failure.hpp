#pragma once

#include <weave/futures/model/evaluation.hpp>
#include <weave/futures/model/thunk.hpp>

#include <weave/result/make/err.hpp>

#include <weave/support/constructor_bases.hpp>

namespace weave::futures::thunks {

template <typename T>
class [[nodiscard]] Failure : support::NonCopyableBase {
 public:
  using ValueType = T;

  explicit Failure(Error with)
      : error_(std::move(with)) {
  }

  // Movable
  Failure(Failure&& that)
      : error_(std::move(that.error_)) {
  }
  Failure& operator=(Failure&&) = default;

 private:
  template <Consumer<ValueType> Cons>
  class FailureEvaluation : support::PinnedBase {
   public:
    FailureEvaluation(Failure fail, Cons& cons) {
      cons.Complete(result::Err(std::move(fail.error_)));
    }
  };

 public:
  template <Consumer<ValueType> Cons>
  Evaluation<Failure, Cons> auto Force(Cons& cons) {
    return FailureEvaluation<Cons>(std::move(*this), cons);
  }

 private:
  Error error_;
};

}  // namespace weave::futures::thunks
