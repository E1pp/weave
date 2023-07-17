#include <weave/fibers/sched/suspend.hpp>
#include <weave/fibers/core/fiber.hpp>

namespace weave::fibers {

void Suspend(Awaiter awaiter) {
  auto* fiber = Fiber::Self();
  WHEELS_VERIFY(fiber != nullptr, "You must be a fiber!");

  fiber->SetAwaiter(awaiter);

  fiber->Suspend();
}

}  // namespace weave::fibers