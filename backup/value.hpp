// #pragma once

// #include <weave/futures/old_model/evaluation.hpp>
// #include <weave/futures/old_model/thunk.hpp>

// #include <weave/result/make/ok.hpp>

// namespace weave::futures::thunks {

// template <typename T>
// class [[nodiscard]] Value {
//  public:
//   using ValueType = T;

//   explicit Value(T&& val)
//       : value_(std::move(val)) {
//   }

//   // Non-Copyable
//   Value(const Value&) = delete;
//   Value& operator=(const Value&) = delete;

//   // Movable
//   // Construction is non-trivial because tl::expected is broken
//   Value(Value&& rhs)
//       : value_(std::move(rhs.value_)) {
//   }
//   Value& operator=(Value&&) = default;

//  private:
//   template <Consumer<ValueType> Cons>
//   class ValueEvaluation {
//    public:
//     // Pinned
//     ValueEvaluation(const ValueEvaluation&) = delete;
//     ValueEvaluation& operator=(const ValueEvaluation&) = delete;

//     ValueEvaluation(ValueEvaluation&&) = delete;
//     ValueEvaluation& operator=(ValueEvaluation&&) = delete;

    
//     ValueEvaluation(Value fut, Cons& consumer) {
//       consumer.Consume(result::Ok(std::move(fut.value_)));
//     }
//   };

//  public:
//   template <Consumer<ValueType> Cons>
//   Evaluation<Value, Cons> auto Force(Cons& consumer){
//     return ValueEvaluation(std::move(*this), consumer);
//   }

//  private:
//   T value_;
// };

// }  // namespace weave::futures::thunks