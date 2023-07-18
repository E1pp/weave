#pragma once

#include <weave/cancel/never.hpp>

#include <weave/coro/core.hpp>

#include <weave/executors/task.hpp>

#include <weave/fibers/core/awaiter.hpp>
#include <weave/fibers/core/handle.hpp>

#include <weave/fibers/core/scheduler.hpp>

#include <twist/ed/local/var.hpp>

namespace weave::fibers {

// Fiber = stackful coroutine + scheduler (thread pool)

class Fiber : public executors::Task,
              public wheels::IntrusiveListNode<Fiber> {
 public:
  // schedules fiber in the executor's queue
  void Schedule(
      executors::SchedulerHint hint = executors::SchedulerHint::UpToYou);

  void SetupFiber(Scheduler* sched, cancel::Token token) {
    my_sched_ = sched;
    my_token_ = token;
  }

  cancel::Token CancelToken() {
    return my_token_;
  }

  // Just throws at this point
  // Use Suspend with function which returns you the handle you wanna switch to
  void Switch();

  void Suspend();

  // executors::Task::Run
  void Run() noexcept override;

  static Fiber* Self();

  template <typename Function>
  requires std::invocable<Function> Fiber(Scheduler& scheduler,
                                          Function function)
      : my_sched_(&scheduler),
        stack_(sure::Stack::AllocateBytes(coro::kDefaultStackSize)),
        my_task_(
            [function = std::move(function)]() mutable noexcept {
              function();
            },
            &stack_) {
  }

  template <typename Function>
  requires std::invocable<Function>
  void GoChild(Function function) {
    Fiber* child = new Fiber(*my_sched_, std::move(function));

    child->Schedule();
  }

  // We require that handle_interface lifetime is at least
  // as long as the duration of Fiber::Run() call
  // which ones work:
  // 1) entire fiber lifetime
  // 2) in the scope of the Suspend call
  void SetAwaiter(const Awaiter&);

  size_t GetEpoch() {
    return epoch_count_;
  }

 private:
  static FiberHandle DefaultAwaiter(FiberHandle);

  // Run fiber with a given handle
  // returns the next handle to run
  static FiberHandle RunLoop(FiberHandle handle);

 private:
  Scheduler* my_sched_;

  sure::Stack stack_;
  coro::Coroutine my_task_;

  Awaiter awaiter_{DefaultAwaiter};
  size_t epoch_count_{0};

  inline TWISTED_THREAD_LOCAL_PTR(Fiber, active_fiber)

      cancel::Token my_token_{cancel::Never()};
};

}  // namespace weave::fibers