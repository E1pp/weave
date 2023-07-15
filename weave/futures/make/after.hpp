#pragma once

#include <weave/futures/old_types/future.hpp>

#include <weave/futures/thunks/make/after.hpp>

#include <weave/satellite/satellite.hpp>

namespace weave::futures {

inline Future<Unit> auto After(timers::Delay delay) {
  return thunks::After{delay};
}

inline Future<Unit> auto After(timers::Millis delay) {
  auto* global_proc = satellite::GetProcessor();

  WHEELS_VERIFY(global_proc != nullptr,
                "Use satellite::MakeVisible before calling this overload!");

  return thunks::After{global_proc->DelayFromThis(delay)};
}

}  // namespace weave::futures