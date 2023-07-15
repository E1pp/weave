#pragma once

#include <weave/futures/old_model/thunk.hpp>

#include <cstdlib>

namespace weave::futures::thunks {

template <typename T>
struct [[nodiscard]] NotImplemented {
  using ValueType = T;

  NotImplemented() = default;

  // Non-copyable
  NotImplemented(const NotImplemented&) = delete;

  // Movable
  NotImplemented(NotImplemented&&) = default;

  void Start(IConsumer<T>*) {
    std::abort();
  }

  void Cancellable() {
    // No-Op
  }
};

}  // namespace weave::futures::thunks
