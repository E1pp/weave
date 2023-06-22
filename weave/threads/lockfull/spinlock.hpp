#pragma once

#include <twist/ed/stdlike/atomic.hpp>
#include <twist/ed/wait/spin.hpp>

namespace weave::threads::lockfull {

// Test-and-TAS spinlock

class SpinLock {
 public:
  SpinLock() = default;

  // Pinned
  SpinLock(const SpinLock&) = delete;
  SpinLock& operator=(const SpinLock&) = delete;

  SpinLock(SpinLock&&) = delete;
  SpinLock& operator=(SpinLock&&) = delete;

  void Lock() {
    twist::ed::SpinWait spin;
    while (locked_.exchange(1, std::memory_order::acquire) == 1) {
      while (locked_.load(std::memory_order::relaxed) == 1) {
        spin();
      }
    }
  }

  bool TryLock() {
    return locked_.exchange(1, std::memory_order::acquire) == 0;
  }

  void Unlock() {
    locked_.store(0, std::memory_order::release);
  }

  // BasicLockable

  void lock() {  // NOLINT
    Lock();
  }

  void unlock() {  // NOLINT
    Unlock();
  }

  bool try_lock() {  // NOLINT
    return TryLock();
  }

 private:
  twist::ed::stdlike::atomic<uint64_t> locked_{0};
};

}  // namespace weave::threads::lockfull
