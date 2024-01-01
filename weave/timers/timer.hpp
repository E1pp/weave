#pragma once

#include <weave/cancel/token.hpp>

#include <weave/executors/task.hpp>

#include <weave/timers/millis.hpp>

namespace weave::timers {

struct ITimer : public executors::ITask {
  virtual ~ITimer() = default;

  virtual Millis GetDelay() = 0;

  // Cancellation
  virtual cancel::Token CancelToken() = 0;
};

struct TimerBase : public ITimer, public wheels::IntrusiveListNode<TimerBase> {
  std::chrono::steady_clock::time_point deadline;
};

}  // namespace weave::timers