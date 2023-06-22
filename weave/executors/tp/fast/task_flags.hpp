#pragma once

#include <cstdint>

namespace weave::executors::tp::fast {

struct TaskFlags {
  enum Flags : uintptr_t { NoFlags = 0, External = 1 };

  static void SetBits(uintptr_t& target, Flags flag) {
    target |= flag;
  }

  static bool IsSet(uintptr_t target, Flags flag) {
    return (target & flag) != 0;
  }

  static void Reset(uintptr_t& target, Flags flag) {
    target ^= flag;
  }
};

}  // namespace weave::executors::tp::fast