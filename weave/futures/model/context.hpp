#pragma once

#include <weave/executors/executor.hpp>
#include <weave/executors/inline.hpp>

namespace weave::futures {

// Represents mutable future state

struct Context {
  executors::IExecutor* executor_;
  executors::SchedulerHint hint_;

  Context(executors::IExecutor* exe, executors::SchedulerHint hint)
      : executor_(exe),
        hint_(hint) {
  }

  Context()
      : executor_(&executors::Inline()),
        hint_(executors::SchedulerHint::Next) {
  }
};

}  // namespace weave::futures
