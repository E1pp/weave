#include <weave/executors/manual.hpp>

namespace weave::executors {

void ManualExecutor::Submit(Task* task, SchedulerHint) {
  queue_.PushBack(task);
}

// Run tasks

size_t ManualExecutor::RunAtMost(size_t limit) {
  size_t tasks_done = 0;

  while (tasks_done < limit && NonEmpty()) {
    queue_.PopFront()->Run();

    ++tasks_done;
  }

  return tasks_done;
}

size_t ManualExecutor::Drain() {
  size_t tasks_done = 0;
  while (!queue_.IsEmpty()) {
    queue_.PopFront()->Run();

    ++tasks_done;
  }

  return tasks_done;
}

}  // namespace weave::executors
