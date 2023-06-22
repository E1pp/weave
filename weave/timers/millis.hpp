#pragma once

#include <chrono>

namespace weave::timers {

using Millis = std::chrono::milliseconds;

template <typename Duration>
inline Millis ToMillis(Duration dur) {
  return std::chrono::duration_cast<Millis>(dur);
}

}  // namespace weave::timers