#include <weave/executors/inline.hpp>

namespace weave::executors {

class InlineExecutor : public IExecutor {
 public:
  // IExecutor
  void Submit(Task* task, SchedulerHint) override {
    task->Run();
  }
};

IExecutor& Inline() {
  static InlineExecutor instance;
  return instance;
}

}  // namespace weave::executors
