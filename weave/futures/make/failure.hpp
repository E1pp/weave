#pragma once

#include <weave/futures/old_types/future.hpp>

#include <weave/futures/thunks/make/failure.hpp>

namespace weave::futures {

template <typename T>
Future<T> auto Failure(Error with) {
  return thunks::Failure<T>(std::move(with));
}

}  // namespace weave::futures
