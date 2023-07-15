#pragma once

#include <weave/futures/types/future.hpp>

#include <weave/futures/thunks/make/value.hpp>

namespace weave::futures {

template <typename T>
Future<T> auto Value(T val) {
  return thunks::Value<T>(std::move(val));
}

}  // namespace weave::futures
