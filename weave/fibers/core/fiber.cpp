#include <weave/fibers/core/fiber.hpp>

#include <wheels/core/panic.hpp>

namespace weave::fibers {

void Fiber::Schedule(executors::SchedulerHint hint) {
  // pre Submit handling bit
  my_sched_->Submit(this, hint);
}

void Fiber::Switch() {
  WHEELS_PANIC(
      "Oh, so you really like switching, huh? That's great, but how about you "
      "\"Switch\" to not using this function?");
}

void Fiber::Suspend() {
  my_task_.Suspend();
}

void Fiber::Run() noexcept {
  auto next_fiber_to_run = FiberHandle(this);

  while ((next_fiber_to_run = RunLoop(next_fiber_to_run)).IsValid()) {
    ;
  }  // exit on getting invalid handle
}

FiberHandle Fiber::RunLoop(FiberHandle handle) {
  // Unpack the actual Fiber
  // invariant: only up to one concurrent FiberRunner
  // call accesses running_fiber's fields

  Fiber* running_fiber = handle.Release();
  running_fiber->SetAwaiter(DefaultAwaiter);

  // save prev active
  Fiber* prev_active = active_fiber;

  // set new active and run it then restore old active
  active_fiber = running_fiber;
  running_fiber->my_task_.Resume();
  active_fiber = prev_active;

  // pessimistically increment epoch count
  running_fiber->epoch_count_++;

  // do the callback
  auto next_fiber = running_fiber->awaiter_(FiberHandle(running_fiber));

  // if next_fiber points to the running_fiber we have to decrease epoch count
  if (next_fiber.fiber_ == running_fiber) {
    running_fiber->epoch_count_--;
  }

  // reschedule in case of symm transfer
  if (next_fiber.IsValid() && next_fiber.fiber_ != running_fiber) {
    running_fiber->Schedule();
  }

  return next_fiber;
}

Fiber* Fiber::Self() {
  return active_fiber;
}

void Fiber::SetAwaiter(const Awaiter& awaiter) {
  awaiter_ = awaiter;
}

FiberHandle Fiber::DefaultAwaiter(FiberHandle handle) {
  auto caller = handle.Release();

  delete caller;
  return FiberHandle::Invalid();
}

}  // namespace weave::fibers