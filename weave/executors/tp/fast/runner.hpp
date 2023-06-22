#pragma once

#include <weave/executors/tp/fast/picker.hpp>

namespace weave::executors {

struct IRunner {
  virtual ~IRunner() = default;

  virtual void RunnerRoutine(IPicker& picker) = 0;
};

}  // namespace weave::executors