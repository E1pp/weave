#pragma once

#include <weave/futures/old_model/thunk.hpp>

#include <weave/result/make/ok.hpp>

namespace weave::futures::thunks {

template <typename T>
class [[nodiscard]] Value {
 public:
  using ValueType = T;

  explicit Value(T&& val)
      : value_(std::move(val)) {
  }

  // Non-Copyable
  Value(const Value&) = delete;
  Value& operator=(const Value&) = delete;

  // Movable
  // Construction is non-trivial because tl::expected is broken
  Value(Value&& rhs)
      : value_(std::move(rhs.value_)) {
  }
  Value& operator=(Value&&) = default;

  void Start(IConsumer<T>* consumer) {
    if (consumer->CancelToken().CancelRequested()) {
      consumer->Cancel(Context{});
    } else {
      consumer->Complete(result::Ok(std::move(value_)));
    }
  }

  void Cancellable() {
    // No-Op
  }

 private:
  T value_;
};

}  // namespace weave::futures::thunks