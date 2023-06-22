#include <weave/fibers/sched/suspend.hpp>
#include <weave/fibers/core/fiber.hpp>

namespace weave::fibers {

void Suspend(Awaiter awaiter) {
  Fiber::Self()->SetAwaiter(awaiter);

  Fiber::Self()->Suspend();
}

}  // namespace weave::fibers