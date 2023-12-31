#pragma once

#include <weave/futures/types/future.hpp>

#include <weave/futures/thunks/make/just.hpp>

namespace weave::futures {

inline Future<Unit> auto Just() {
  return thunks::Just{};
}

}  // namespace weave::futures
