#pragma once

#include <weave/executors/executor.hpp>

#include <wheels/intrusive/list.hpp>

namespace weave::executors {

// Single-threaded task queue

class ManualExecutor : public IExecutor {
 public:
  ManualExecutor() = default;

  // Non-copyable
  ManualExecutor(const ManualExecutor&) = delete;
  ManualExecutor& operator=(const ManualExecutor&) = delete;

  // Non-movable
  ManualExecutor(ManualExecutor&&) = delete;
  ManualExecutor& operator=(ManualExecutor&&) = delete;

  // IExecutor
  void Submit(Task*, SchedulerHint) override;

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

 private:
  wheels::IntrusiveList<Task> queue_{};
};

}  // namespace weave::executors
