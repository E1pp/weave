#pragma once

#include <weave/futures/model/evaluation.hpp>

#include <weave/result/make/ok.hpp>

#include <weave/support/constructor_bases.hpp>

namespace weave::futures::thunks {

template <typename T>
class [[nodiscard]] Value final : public support::NonCopyableBase {
 public:
  using ValueType = T;

  explicit Value(T&& val) noexcept
      : value_(std::move(val)) {
  }

  // Movable
  // Construction is non-trivial because tl::expected is broken
  Value(Value&& rhs) noexcept
      : value_(std::move(rhs.value_)) {
  }
  Value& operator=(Value&&) = delete;

 private:
  template <Consumer<ValueType> Cons>
  class EvaluationFor final : public support::PinnedBase {
    friend class Value;

    EvaluationFor(Value fut, Cons& consumer)
        : val_(std::move(fut.value_)),
          consumer_(consumer) {
    }

   public:
    void Start() {
      if (consumer_.CancelToken().CancelRequested()) {
        consumer_.Cancel(Context{});
      } else {
        Complete(consumer_, result::Ok(std::move(val_)));
      }
    }

   private:
    ValueType val_;
    Cons& consumer_;
  };

 public:
  template <Consumer<ValueType> Cons>
  Evaluation<Value, Cons> auto Force(Cons& consumer) {
    return EvaluationFor<Cons>(std::move(*this), consumer);
  }

  void Cancellable() {
    // No-Op
  }

 private:
  T value_;
};

}  // namespace weave::futures::thunks