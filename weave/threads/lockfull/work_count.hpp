#pragma once

#include <weave/threads/lockfull/stdlike/mutex.hpp>
#include <weave/threads/lockfull/spinlock.hpp>

#include <twist/ed/stdlike/atomic.hpp>
#include <twist/ed/wait/sys.hpp>

#if !defined(TWIST_FIBERS) && LINUX
#define CAN_RUN_UNSAFE_WAITGROUP
#endif

#if defined(CAN_RUN_UNSAFE_WAITGROUP)
#include <weave/threads/lockfull/wait_group.hpp>
#endif

namespace weave::threads::lockfull {

class WorkCountImpl1;
class WorkCountImpl2;

#if defined(CAN_RUN_UNSAFE_WAITGROUP)
using WorkCount = WorkCountImpl2;
#else
using WorkCount = WorkCountImpl1;
#endif

// Unfortunate spinlock
class WorkCountImpl1 {
  enum State : uint32_t { NoWork = 0, SomeWork = 1 };

 public:
  void Add(size_t count) {
    if (count == 0) {
      return;
    }

    threads::lockfull::stdlike::LockGuard locker(spinlock_);

    if (counter_.fetch_add(count, std::memory_order::relaxed) == 0) {
      flag_.store(State::SomeWork, std::memory_order::relaxed);
    }
  }

  void StealthAdd(size_t count) {
    if (count == 0) {
      return;
    }

    counter_.fetch_add(count, std::memory_order::relaxed);
  }

  void Done(size_t count) {
    if (count == 0) {
      return;
    }

    threads::lockfull::stdlike::LockGuard locker(spinlock_);
    // reduced counter to zero
    if (counter_.fetch_sub(count, std::memory_order::acq_rel) == count) {
      auto wake_key = twist::ed::futex::PrepareWake(flag_);

      flag_.store(State::NoWork, std::memory_order::release);

      twist::ed::futex::WakeAll(wake_key);
    }
  }

  void StealthDone(size_t count) {
    if (count == 0) {
      return;
    }

    counter_.fetch_sub(count, std::memory_order::release);
  }

  void Wait() {
    twist::ed::futex::Wait(flag_, State::SomeWork, std::memory_order::acquire);
  }

 private:
  twist::ed::stdlike::atomic<uint32_t> flag_{State::NoWork};
  twist::ed::stdlike::atomic<uint64_t> counter_{0};

  threads::lockfull::SpinLock spinlock_;
};

#if defined(CAN_RUN_UNSAFE_WAITGROUP)

class WorkCountImpl2 {
 public:
  void Add(size_t count) {
    impl_.Add(count);
  }

  void StealthAdd(size_t count) {
    Add(count);
  }

  void Done(size_t count) {
    impl_.Done(count);
  }

  void StealthDone(size_t count) {
    Done(count);
  }

  void Wait() {
    impl_.Wait();
  }

 private:
  threads::lockfull::WG impl_{};
};

#endif

}  // namespace weave::threads::lockfull