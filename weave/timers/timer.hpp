#pragma once

#include <weave/cancel/token.hpp>

#include <weave/executors/task.hpp>

#include <weave/timers/millis.hpp>

namespace weave::timers {

struct ITimer : public executors::Task {
  virtual ~ITimer() = default;

  virtual Millis GetDelay() = 0;

  // Cancellation
  virtual cancel::Token CancelToken() = 0;
};

}  // namespace weave::timers