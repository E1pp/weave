#pragma once

#include <weave/fibers/core/fiber.hpp>
#include <weave/fibers/core/scheduler.hpp>

namespace weave::fibers {

// Considered harmful

template <typename Function>
void Go(Scheduler& scheduler, Function routine) {
  Fiber* newbie = new Fiber(scheduler, std::move(routine));

  newbie->Schedule();
}

template <typename Function>
void Go(Function routine) {
  Fiber::Self()->GoChild(std::move(routine));
}

}  // namespace weave::fibers