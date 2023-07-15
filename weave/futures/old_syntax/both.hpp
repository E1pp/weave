#pragma once

#include <weave/futures/combine/par/all.hpp>

template <weave::futures::SomeFuture LeftFuture,
          weave::futures::SomeFuture RightFuture>
auto operator+(LeftFuture f, RightFuture g) {
  return weave::futures::Both(std::move(f), std::move(g));
}
