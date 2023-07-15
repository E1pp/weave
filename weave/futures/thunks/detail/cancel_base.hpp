#pragma once

#include <weave/futures/old_traits/cancellable.hpp>

namespace weave::futures::thunks::detail {

// Not Cancellable
template <SomeFuture Future>
struct CancellableBase {};

// Cancellable
template <traits::Cancellable Future>
struct CancellableBase<Future> {
  void Cancellable() {
    // No-Op
  }
};

// Not Cancellable
template <typename... Ts>
struct VariadicCancellableBase {};

// Cancellable
template <traits::Cancellable First, traits::Cancellable... Suffix>
struct VariadicCancellableBase<First, Suffix...> {
  void Cancellable() {
    // No-Op
  }
};

template <typename T>
struct JustCancellableBase {};

template <traits::JustCancellable T>
struct JustCancellableBase<T> {
  void Cancellable() {
    // No-Op
  }
};

}  // namespace weave::futures::thunks::detail