#include <weave/fibers/core/fiber.hpp>

#include <weave/fibers/core/self.hpp>

namespace weave::fibers {

Fiber* Self() {
  return Fiber::Self();
}

bool IAmFiber() {
  return Fiber::Self() != nullptr;
}

}  // namespace weave::fibers