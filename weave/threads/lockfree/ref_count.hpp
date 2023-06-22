#pragma once

#include <twist/ed/stdlike/atomic.hpp>

namespace weave::threads::lockfree {

template <typename T, bool OnHeap>
class RefCounter {
 public:
  void AddRef(uint64_t count) {
    ref_count_.fetch_add(count, std::memory_order::relaxed);
  }

  void ReleaseRef() {
    if (ref_count_.fetch_sub(1, std::memory_order::acq_rel) == 1) {
      delete static_cast<T*>(this);
    }
  }

 private:
  twist::ed::stdlike::atomic<uint64_t> ref_count_{0};
};

template <typename T>
class RefCounter<T, false> {
 public:
  void AddRef(uint64_t) {
  }

  void ReleaseRef() {
  }
};

}  // namespace weave::threads::lockfree