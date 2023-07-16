#pragma once

#include <weave/futures/combine/par/first.hpp>

template <weave::futures::SomeFuture LeftFuture,
          weave::futures::SomeFuture RightFuture>
auto operator||(LeftFuture f, RightFuture g) {
  return weave::futures::First(std::move(f), std::move(g));
}
