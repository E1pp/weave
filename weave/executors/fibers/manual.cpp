#include <weave/executors/fibers/manual.hpp>
#include <weave/executors/prison.hpp>

#include <weave/fibers/core/fiber.hpp>

#include <weave/fibers/sched/suspend.hpp>

namespace weave::executors::fibers {

using Task = weave::executors::Task;
using SchedulerHint = weave::executors::SchedulerHint;

void ManualExecutor::Submit(Task* task, SchedulerHint) {
  queue_.PushBack(task);
}

size_t ManualExecutor::RunAtMost(size_t limit) {
  limit_ = limit;
  while (limit_ > 0 && queue_.HasItems()) {
    weave::fibers::Fiber* carrier = GetCarrier();
    carrier->Run();
  }

  return limit - limit_;
}

size_t ManualExecutor::Drain() {
  size_t total = 0;
  size_t iter_ran = 0;

  while ((iter_ran = RunAtMost(std::numeric_limits<size_t>::max())) != 0) {
    WHEELS_ASSERT(iter_ran > 0, "Negative tasks ran");
    total += iter_ran;
  }

  return total;
}

weave::fibers::Fiber* ManualExecutor::GetCarrier() {
  if (pool_.IsEmpty()) {
    return new Fiber(executors::Prison(), [&]() {
      RunManualRoutine();
    });
  }

  return pool_.PopFront();
}

void ManualExecutor::RunManualRoutine() {
  Fiber* carrier = Fiber::Self();
  WHEELS_ASSERT(carrier != nullptr, "Outside of Fiber context!");

  while (!StopRequested()) {
    while (auto task = queue_.PopFront()) {
      size_t epoch = carrier->GetEpoch();
      limit_--;
      task->Run();

      if (epoch != carrier->GetEpoch() || limit_ == 0) {
        break;
      }
    }

    // short path when Stop was already called
    if (StopRequested()) {
      return;
    }

    RetireAsCarrier(carrier);
  }
}

void ManualExecutor::RetireAsCarrier(Fiber* carrier) {
  auto awaiter = [&](weave::fibers::FiberHandle) {
    pool_.PushBack(carrier);

    return weave::fibers::FiberHandle::Invalid();
  };

  weave::fibers::Suspend(awaiter);
}

void ManualExecutor::Stop() {
  stopped_ = true;
  WHEELS_ASSERT(queue_.IsEmpty(), "Stopping while work is not done!");

  while (auto carrier = pool_.PopFront()) {
    // "Joining" carriers
    carrier->Run();
  }
}

}  // namespace weave::executors::fibers