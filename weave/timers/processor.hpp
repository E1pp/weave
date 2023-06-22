#pragma once

#include <weave/timers/delay.hpp>
#include <weave/timers/timer.hpp>

#include <weave/satellite/satellite.hpp>

namespace weave::timers {

struct IProcessor {
  virtual ~IProcessor() = default;

  virtual void AddTimer(ITimer*) = 0;

  virtual Delay DelayFromThis(Millis) = 0;

  void MakeGlobal() {
    satellite::MakeVisible(this);
  }
};

}  // namespace weave::timers