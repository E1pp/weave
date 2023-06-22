#include <weave/fibers/sched/yield.hpp>

#include <weave/fibers/sched/suspend.hpp>

#include <weave/satellite/satellite.hpp>

namespace weave::fibers {

void Yield() {
  satellite::PollToken();

  auto awaiter = [](FiberHandle handle) {
    handle.Schedule(executors::SchedulerHint::Last);
    return FiberHandle::Invalid();
  };

  Suspend(awaiter);

  satellite::PollToken();
}

}  // namespace weave::fibers