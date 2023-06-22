#pragma once

namespace weave::executors {

enum class SchedulerHint {
  UpToYou = 1,  // Rely on executor scheduling decision
  Next = 2,     // Use LIFO scheduling
  Last = 3      // Yield control to every other task
};

}  // namespace weave::executors
