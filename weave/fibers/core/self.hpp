#pragma once

#include <weave/fibers/core/fwd.hpp>

namespace weave::fibers {

// Ptr to current fiber
Fiber* Self();

// true iff in fiber context
bool IAmFiber();

}  // namespace weave::fibers