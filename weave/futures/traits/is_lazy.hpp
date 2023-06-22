#pragma once

#include <weave/futures/types/future.hpp>

namespace weave::futures::traits {

template <typename Future>
concept Eager = SomeFuture<Future> && requires(Future f) {
  f.ImEager();
};

template <typename Future>
concept Lazy = SomeFuture<Future> && !Eager<Future>;

}  // namespace weave::futures::traits
