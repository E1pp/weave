#pragma once

#include <weave/futures/model/evaluation.hpp>

#include <weave/result/make/err.hpp>

#include <weave/support/constructor_bases.hpp>

namespace weave::futures::thunks {

template <typename T>
class [[nodiscard]] Failure final : public support::NonCopyableBase {
 public:
  using ValueType = T;

  explicit Failure(Error with)
      : error_(std::move(with)) {
  }

  // Movable
  Failure(Failure&& that) noexcept
      : error_(std::move(that.error_)) {
  }
  Failure& operator=(Failure&&) = default;

 private:
  template <Consumer<ValueType> Cons>
  class EvaluationFor final : public support::PinnedBase {
   public:
    EvaluationFor(Failure fut, Cons& consumer)
        : err_(std::move(fut.error_)),
          consumer_(consumer) {
    }

    void Start() {
      if (consumer_.CancelToken().CancelRequested()) {
        consumer_.Cancel(Context{});
      } else {
        Complete<ValueType>(consumer_, result::Err(std::move(err_)));
      }
    }

   private:
    Error err_;
    Cons& consumer_;
  };

 public:
  template <Consumer<ValueType> Cons>
  Evaluation<Failure, Cons> auto Force(Cons& cons) {
    return EvaluationFor<Cons>(std::move(*this), cons);
  }

  void Cancellable(){
    // No-Op
  }

 private:
  Error error_;
};

}  // namespace weave::futures::thunks
