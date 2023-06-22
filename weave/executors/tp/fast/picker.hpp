#pragma once

#include <weave/executors/task.hpp>

namespace weave::executors {

struct IPicker {
  virtual ~IPicker() = default;

  virtual bool StopRequested() const = 0;

  virtual Task* PickTask() = 0;
};

}  // namespace weave::executors