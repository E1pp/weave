#pragma once

#include <weave/futures/types/future.hpp>

#include <weave/futures/thunks/make/never.hpp>

namespace weave::futures {

inline Future<Unit> auto Never() {
  return thunks::Never{};
}

}  // namespace weave::futures