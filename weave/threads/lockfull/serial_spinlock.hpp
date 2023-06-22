#pragma once

#include <twist/ed/stdlike/atomic.hpp>
#include <twist/ed/wait/spin.hpp>
#include <twist/ed/spin/wait.hpp>

namespace exe::threads::lockfull {

class SerialSpinLock {
 public:
  SerialSpinLock() = default;

  // Pinned
  SerialSpinLock(const SerialSpinLock&) = delete;
  SerialSpinLock& operator=(const SerialSpinLock&) = delete;

  SerialSpinLock(SerialSpinLock&&) = delete;
  SerialSpinLock& operator=(SerialSpinLock&&) = delete;

  class Guard {
    friend class SerialSpinLock;

   public:
    explicit Guard(SerialSpinLock& host)
        : host_(host) {
      host_.Acquire(this);
    }

    ~Guard() {
      host_.Release(this);
    }

   private:
    SerialSpinLock& host_;
    alignas(64) twist::ed::stdlike::atomic<Guard*> next_{nullptr};
    alignas(64) twist::ed::stdlike::atomic<bool> is_owner_{false};
  };

 private:
  void Acquire(Guard* waiter) {
    auto prev_tail = tail_.exchange(waiter, std::memory_order::acq_rel);
    if (prev_tail == nullptr) {
      return;
    }

    // Release call is spinning until we do this line
    prev_tail->next_.store(waiter, std::memory_order::release);

    twist::ed::SpinWait spin;
    while (!waiter->is_owner_.load(std::memory_order::acquire)) {
      // twist::ed::CpuRelax();
      spin();
    }
  }

  void Release(Guard* owner) {
    auto local_next = owner->next_.load(std::memory_order::acquire);
    auto local_owner = owner;

    if (local_next == nullptr) {
      // CAS invalidates cache lines for everyone
      if (tail_.compare_exchange_strong(
              local_owner, nullptr, std::memory_order::release,
              std::memory_order::relaxed)) {  // empty queue
        return;
      }

      twist::ed::SpinWait spin;
      while ((local_next = owner->next_.load(std::memory_order::acquire)) ==
             nullptr) {
        spin();
        // twist::ed::CpuRelax();
      }
    }

    // at this point local_next != nullptr
    local_next->is_owner_.store(true, std::memory_order::release);
  }

 private:
  twist::ed::stdlike::atomic<Guard*> tail_{nullptr};
};

}  // namespace exe::threads::lockfull