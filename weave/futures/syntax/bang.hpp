#pragma once

#include <weave/futures/types/future.hpp>

#include <weave/futures/combine/seq/start.hpp>

/*
 * "Bang" operator (!)
 *
 * Named after bang patterns in Strict Haskell
 * https://www.fpcomplete.com/haskell/tutorial/all-about-strictness/
 *
 */

template <weave::futures::SomeFuture InputFuture>
auto operator!(InputFuture f) {
  return std::move(f) | weave::futures::Start();
}
