#pragma once

#include <weave/futures/model/thunk.hpp>

#include <weave/futures/thunks/combine/seq/detail/start_state.hpp>

#include <weave/futures/thunks/detail/cancel_base.hpp>

namespace weave::futures::thunks {

template <SomeFuture Future>
class [[nodiscard]] StartedFuture final
    : public detail::CancellableBase<Future> {
 public:
  using ValueType = typename Future::ValueType;
  using SharedState = detail::SharedState<ValueType>;

  explicit StartedFuture(Future future) {
    state_ = new detail::StartState(std::move(future));
  }

  // Non-copyable
  StartedFuture(const StartedFuture&) = delete;
  StartedFuture& operator=(const StartedFuture&) = delete;

  // Movable
  StartedFuture(StartedFuture&& that)
      : state_(that.Release()) {
  }

  void Start(IConsumer<ValueType>* consumer) {
    Release()->Consume(consumer);
  }

  void RequestCancel() && {
    Release()->Forward(cancel::Signal::Cancel());
  }

  void ImEager() {
    // No-Op
  }

  ~StartedFuture() {
    assert(state_ == nullptr);
  }

 private:
  SharedState* Release() {
    return std::exchange(state_, nullptr);
  }

 private:
  SharedState* state_{nullptr};
};

}  // namespace weave::futures::thunks
