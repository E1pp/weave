#pragma once

#include <weave/futures/old_types/future.hpp>

#include <weave/futures/thunks/make/never.hpp>

namespace weave::futures {

// Future which never completes
inline Future<Unit> auto Never() {
  return thunks::Never{};
}

}  // namespace weave::futures