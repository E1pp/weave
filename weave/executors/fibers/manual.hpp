#pragma once

#include <weave/executors/executor.hpp>
#include <weave/executors/task.hpp>

#include <weave/fibers/core/fwd.hpp>

#include <cstdlib>

namespace weave::executors::fibers {

// ManualExecutor which runs tasks in Fiber context
class ManualExecutor : public IExecutor {
  using Fiber = weave::fibers::Fiber;

 public:
  ManualExecutor() = default;

  // Non-copyable
  ManualExecutor(const ManualExecutor&) = delete;
  ManualExecutor& operator=(const ManualExecutor&) = delete;

  // Non-movable
  ManualExecutor(ManualExecutor&&) = delete;
  ManualExecutor& operator=(ManualExecutor&&) = delete;

  // IExecutor
  void Submit(Task*, SchedulerHint hint = SchedulerHint::UpToYou) override;

  // Run tasks

  // Run at most `limit` tasks from queue
  // Returns number of completed tasks
  size_t RunAtMost(size_t limit);

  // Run next task if queue is not empty
  bool RunNext() {
    return RunAtMost(1) == 1;
  }

  // Run tasks until queue is empty
  // Returns number of completed tasks
  // Post-condition: IsEmpty() == true
  size_t Drain();

  size_t TaskCount() const {
    return queue_.Size();
  }

  bool IsEmpty() const {
    return queue_.IsEmpty();
  }

  bool NonEmpty() const {
    return !IsEmpty();
  }

  void Stop();

 private:
  Fiber* GetCarrier();

  void RunManualRoutine();

  bool StopRequested() {
    return stopped_;
  }

  void RetireAsCarrier(Fiber*);

 private:
  wheels::IntrusiveList<Task> queue_{};
  wheels::IntrusiveList<Fiber> pool_{};
  size_t limit_{0};
  bool stopped_{false};
};

}  // namespace weave::executors::fibers