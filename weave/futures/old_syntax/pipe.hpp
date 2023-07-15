#pragma once

#include <weave/futures/old_types/naked.hpp>

template <weave::futures::SomeFuture Future, typename C>
auto operator|(Future f, C c) {
  return c.Pipe(std::move(f));
}
