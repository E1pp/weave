#pragma once

#include <weave/timers/fwd.hpp>

#include <chrono>

namespace weave::fibers {

using Millis = std::chrono::milliseconds;

void SleepFor(Millis);

void SleepFor(timers::Delay);

}  // namespace weave::fibers