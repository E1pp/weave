#pragma once

#include <weave/cancel/token.hpp>

#include <weave/timers/millis.hpp>

#include <wheels/intrusive/list.hpp>

namespace weave::timers {

struct ITimer : wheels::IntrusiveListNode<ITimer> {
  virtual ~ITimer() = default;

  virtual Millis GetDelay() = 0;

  // Called when timer expires
  virtual void Callback() = 0;
};

}  // namespace weave::timers