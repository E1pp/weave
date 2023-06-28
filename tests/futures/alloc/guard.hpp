#pragma once

#include <wheels/core/assert.hpp>

#include <cstdlib>

#include <atomic>

static std::atomic<size_t> alloc_count{0};

#if !(__has_feature(address_sanitizer) || defined(__SANITIZE_ADDRESS__))

void* operator new(size_t size) {
  alloc_count.fetch_add(1, std::memory_order::relaxed);
  return std::malloc(size);
}

#endif

size_t AllocationCount() {
  return alloc_count.load(std::memory_order::relaxed);
}

struct AllocationGuard {
  AllocationGuard()
    : before_(AllocationCount()) {
  }

  ~AllocationGuard() {
    size_t after = AllocationCount();
    WHEELS_VERIFY(before_ == after, "Allocations!");
  }

  size_t before_;
};
