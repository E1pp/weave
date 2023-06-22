#pragma once

#include <weave/futures/model/thunk.hpp>

#include <weave/result/make/err.hpp>

namespace weave::futures::thunks {

template <typename T>
class [[nodiscard]] Failure {
 public:
  using ValueType = T;

  explicit Failure(Error with)
      : error_(std::move(with)) {
  }

  // Non-copyable
  Failure(const Failure&) = delete;
  Failure& operator=(const Failure&) = delete;

  // Movable
  Failure(Failure&& that)
      : error_(std::move(that.error_)) {
  }
  Failure& operator=(Failure&&) = default;

  void Start(IConsumer<T>* consumer) {
    if (consumer->CancelToken().CancelRequested()) {
      consumer->Cancel(Context{});
    } else {
      consumer->Complete(result::Err(std::move(error_)));
    }
  }

  void Cancellable() {
    // No-Op
  }

 private:
  Error error_;
};

}  // namespace weave::futures::thunks
