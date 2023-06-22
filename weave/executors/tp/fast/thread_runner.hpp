#pragma once

#include <weave/executors/tp/fast/runner.hpp>

namespace weave::executors::runners {

class ThreadRunner final : public IRunner {
 public:
  void RunnerRoutine(IPicker& picker) override final {
    while (Task* task = picker.PickTask()) {
      task->Run();
    }
  }

  static ThreadRunner& Instance() {
    static ThreadRunner instance;
    return instance;
  }
};

}  // namespace weave::executors::runners