#pragma once

#include <weave/futures/model/thunk.hpp>

#include <weave/futures/thunks/detail/shared_state.hpp>

namespace weave::futures::thunks {

// Always Cancellable
template <typename T>
class [[nodiscard]] ContractFuture {
 public:
  using ValueType = T;
  using SharedState = detail::SharedState<T>;

  explicit ContractFuture(SharedState* state)
      : state_(state) {
  }

  // Non-copyable
  ContractFuture(const ContractFuture&) = delete;
  ContractFuture& operator=(const ContractFuture&) = delete;

  // Movable
  ContractFuture(ContractFuture&& that)
      : state_(that.Release()) {
  }

  void Start(IConsumer<T>* consumer) {
    Release()->Consume(consumer);
  }

  void RequestCancel() && {
    Release()->Forward(cancel::Signal::Cancel());
  }

  void ImEager() {
    // No-Op
  }

  void Cancellable() {
    // No-Op
  }

  ~ContractFuture() {
    assert(state_ == nullptr);
  }

 private:
  SharedState* Release() {
    return std::exchange(state_, nullptr);
  }

 private:
  SharedState* state_;
};

}  // namespace weave::futures::thunks