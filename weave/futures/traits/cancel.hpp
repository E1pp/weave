#pragma once

#include <weave/futures/types/future.hpp>

namespace weave::futures::traits {

template <typename T>
concept JustCancellable = requires(T obj) {
  { obj.Cancellable() } -> std::same_as<void>;
};

template <typename Future>
concept Cancellable = Thunk<Future> && JustCancellable<Future>;

}  // namespace weave::futures::traits