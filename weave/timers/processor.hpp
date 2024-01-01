#pragma once

#include <weave/timers/delay.hpp>
#include <weave/timers/timer.hpp>

#include <weave/satellite/satellite.hpp>

#include <optional>

namespace weave::timers {

struct IProcessor {
  virtual ~IProcessor() = default;

  virtual void AddTimer(TimerBase*) = 0;

  virtual void NotifyProcessor() = 0;

  Delay DelayFromThis(Millis ms) {
    return Delay{ms, *this};
  }

  // Allow Timers to deduce processor automatically
  void MakeGlobal() {
    satellite::MakeVisible(this);
  }
};

}  // namespace weave::timers