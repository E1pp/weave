#pragma once

#include <weave/futures/model/evaluation.hpp>
#include <weave/futures/model/thunk.hpp>

#include <weave/result/make/ok.hpp>

#include <weave/support/constructor_bases.hpp>

namespace weave::futures::thunks {

template <typename T>
class [[nodiscard]] Value : support::NonCopyableBase {
 public:
  using ValueType = T;

  explicit Value(T&& val)
      : value_(std::move(val)) {
  }

  // Movable
  // Construction is non-trivial because tl::expected is broken
  Value(Value&& rhs)
      : value_(std::move(rhs.value_)) {
  }
  Value& operator=(Value&&) = default;

 private:
  template <Consumer<ValueType> Cons>
  class ValueEvaluation : support::PinnedBase {
   public:
    ValueEvaluation(Value fut, Cons& consumer) {
      consumer.Complete(result::Ok(std::move(fut.value_)));
    }
  };

 public:
  template <Consumer<ValueType> Cons>
  Evaluation<Value, Cons> auto Force(Cons& consumer) {
    return ValueEvaluation<Cons>(std::move(*this), consumer);
  }

 private:
  T value_;
};

}  // namespace weave::futures::thunks