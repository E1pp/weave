#include <weave/executors/prison.hpp>

#include <wheels/core/panic.hpp>

namespace weave::executors {

class PrisonExecutor : public IExecutor {
 public:
  void Submit(Task*, SchedulerHint) override {
    WHEELS_PANIC("Task has been submitted to Prison");
  }
};

IExecutor& Prison() {
  static PrisonExecutor instance;

  return instance;
}

}  // namespace weave::executors