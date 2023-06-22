#include <weave/executors/prison.hpp>

#include <weave/fibers/core/fiber.hpp>

#include <weave/fibers/sched/suspend.hpp>

#include <weave/executors/fibers/tp/fiber_runner.hpp>

#include <twist/ed/local/ptr.hpp>

namespace weave::executors::runners {

TWISTED_THREAD_LOCAL_PTR(FiberRunnerShard, current);

void FiberRunner::RunnerRoutine(IPicker& picker) {
  // this stack frame exists until stop is requested
  FiberRunnerShard shard(&picker);
  current = &shard;

  shard.Run();
  // unreachable until StopRequested() == true;

  shard.Stop();
}

//////////////////////////////////////////////////////////////////////

weave::fibers::Fiber* FiberRunnerShard::GetCarrier() {
  if (pool_.IsEmpty()) {
    return new Fiber(executors::Prison(), [&] {
      CarrierRoutine();
    });
  }

  return pool_.PopFront();
}

void FiberRunnerShard::Run() {
  while (!picker_->StopRequested()) {
    Fiber* carrier = GetCarrier();

    carrier->Run();
  }
}

void FiberRunnerShard::CarrierRoutine() {
  Fiber* carrier = Fiber::Self();
  FiberRunnerShard* owner = CurrentShard();

  WHEELS_ASSERT(carrier != nullptr, "Outside of fibers context!");

  while (!owner->StopRequested()) {
    while (Task* task = owner->picker_->PickTask()) {
      auto epoch = carrier->GetEpoch();

      task->Run();
      //
      if (epoch != carrier->GetEpoch()) {
        // we have been suspended and possibly stolen
        // we must update our owner
        owner = CurrentShard();
        break;
      }
    }

    // short path when the work is already over
    if (owner->StopRequested()) {
      return;
    }

    // if we have been stolen, thus we retire to the new owner's pool
    owner->RetireAsCarrier(carrier);
  }
}

void FiberRunnerShard::RetireAsCarrier(Fiber* carrier) {
  auto awaiter = [&](weave::fibers::FiberHandle) {
    pool_.PushBack(carrier);

    return weave::fibers::FiberHandle::Invalid();
  };

  weave::fibers::Suspend(awaiter);
}

void FiberRunnerShard::Stop() {
  WHEELS_ASSERT(picker_->StopRequested(), "Stopping when picker is not done!");
  stopped_ = true;

  while (auto carrier = pool_.PopFront()) {
    // Join carriers
    carrier->Run();
  }
}

FiberRunnerShard* FiberRunnerShard::CurrentShard() {
  return current;
}

}  // namespace weave::executors::runners