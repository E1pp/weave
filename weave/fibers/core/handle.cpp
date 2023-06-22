#include <weave/fibers/core/handle.hpp>

#include <weave/fibers/core/fiber.hpp>

#include <wheels/core/assert.hpp>

#include <utility>

namespace weave::fibers {

Fiber* FiberHandle::Release() {
  WHEELS_ASSERT(IsValid(), "Invalid fiber handle");
  return std::exchange(fiber_, nullptr);
}

void FiberHandle::Schedule(executors::SchedulerHint hint) {
  Release()->Schedule(hint);
}

void FiberHandle::Switch() {
  Release()->Switch();
}

}  // namespace weave::fibers
