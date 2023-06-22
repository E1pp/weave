#pragma once

#include <twist/ed/stdlike/atomic.hpp>

#include <wheels/core/assert.hpp>

namespace weave::threads::lockfree {

// First NumFlags bits are for flags
// Flags are 1-indexed
template <size_t NumFlags>
class CountAndFlags {
  static_assert(NumFlags <= 32);

 public:
  // Count Manipulation

  // fetch_add(count)
  uint64_t AddCount(uint32_t count) {
    return state_.fetch_add(count << NumFlags, std::memory_order::relaxed);
  }

  // fetch_sub(1)
  uint64_t RemoveCount() {
    return state_.fetch_sub(1 << NumFlags, std::memory_order::acq_rel);
  }

  // Flag Manipulation

  template <size_t Index>
  uint64_t ActivateFlag() {
    static_assert(Index <= NumFlags);
    return state_.fetch_or(1 << (Index - 1), std::memory_order::relaxed);
  }

  // MultiWord Manipulation <- Unsafe!!!

  // Precondition: Index's Flag was inactive
  template <size_t Index>
  uint64_t AddCountAndActivateFlag(uint32_t count) {
    static_assert(Index <= NumFlags);
    // +(count << NumFlags) ~ Count += count
    // +(1 << (Index - 1)) ~ 0 -> 1 on Index's position
    return state_.fetch_add(((count << NumFlags) + (1 << (Index - 1))),
                            std::memory_order::relaxed);
  }

  // Precondition: Index's Flag was active
  template <size_t Index>
  uint64_t RemoveCountAndDeactivateFlag() {
    static_assert(Index <= NumFlags);
    // -(1 << NumFlags) ~ Count -= 1
    // -(1 << (Index - 1)) ~ 1 -> 0 on Index's position
    return state_.fetch_sub(((1 << NumFlags) + (1 << (Index - 1))),
                            std::memory_order::acq_rel);
  }

  // Interpretation

  template <size_t Index>
  static bool ReadFlag(uint64_t state) {
    static_assert(Index <= NumFlags);
    // shift by Index - 1 to the left to remove data from prev flags
    // then & 1
    return static_cast<bool>((state >> (Index - 1)) & 1);
  }

  static uint64_t ReadCount(uint64_t state) {
    return state >> NumFlags;
  }

 private:
  twist::ed::stdlike::atomic<uint64_t> state_{0};
};

}  // namespace weave::threads::lockfree