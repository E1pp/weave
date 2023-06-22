#pragma once

#include <weave/executors/task.hpp>

#include <weave/executors/hint.hpp>

namespace weave::executors {

// Executors are to function execution as allocators are to memory allocation

struct IExecutor {
  virtual ~IExecutor() = default;

  virtual void Submit(Task* task,
                      SchedulerHint hint = SchedulerHint::UpToYou) = 0;
};

}  // namespace weave::executors
