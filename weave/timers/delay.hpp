#pragma once

#include <weave/timers/fwd.hpp>
#include <weave/timers/millis.hpp>

namespace weave::timers {

// Delay with reference to its processor

struct Delay {
 public:
  explicit Delay(Millis ms, IProcessor& proc)
      : time_(ms),
        processor_(&proc) {
  }

  template <typename Duration>
  explicit Delay(Duration dur, IProcessor& proc)
      : time_(ToMillis(dur)),
        processor_(&proc) {
  }

  Millis time_;
  IProcessor* processor_;
};

}  // namespace weave::timers