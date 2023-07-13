#pragma once

#include <twist/ed/stdlike/atomic.hpp>
#include <twist/ed/wait/sys.hpp>

#include <cstdint>

namespace weave::threads::blocking::stdlike {

class Mutex {
 public:
  Mutex() = default;

  // Pinned
  Mutex(const Mutex&) = delete;
  Mutex& operator=(const Mutex&) = delete;

  Mutex(Mutex&&) = delete;
  Mutex& operator=(Mutex&&) = delete;

  void Lock();

  bool TryLock() {
    return CompareExchange(0, 2) == 0;
  }

  void Unlock();

  // BasicLockable
  // https://en.cppreference.com/w/cpp/named_req/BasicLockable

  void lock() {  // NOLINT
    Lock();
  }

  void unlock() {  // NOLINT
    Unlock();
  }

 private:
  uint32_t CompareExchange(uint32_t expected, uint32_t desired) {
    state_.compare_exchange_strong(expected, desired);
    return expected;
  }

 private:
  // 0 -- unlocked
  // 1 -- locked and there are no waiters
  // 2 -- locked and there are waiters
  twist::ed::stdlike::atomic<uint32_t> state_{0};
};

template <typename Mutex>
class LockGuard {
 public:
  explicit LockGuard(Mutex& mutex)
      : mutex_(&mutex) {
    mutex_->lock();
  }

  // Pinned
  LockGuard(const LockGuard&) = delete;
  LockGuard& operator=(const LockGuard&) = delete;

  LockGuard(LockGuard&&) = delete;
  LockGuard& operator=(LockGuard&&) = delete;

  ~LockGuard() {
    mutex_->unlock();
  }

 private:
  Mutex* mutex_;
};

template <typename Mutex>
class UniqueLock {
 public:
  explicit UniqueLock(Mutex& mutex)
      : mutex_(&mutex) {
    mutex_->lock();
  }

  // Non-copyable
  UniqueLock(const UniqueLock&) = delete;
  UniqueLock& operator=(const UniqueLock&) = delete;

  // Move-constructible
  UniqueLock(UniqueLock&& that)
      : mutex_(that.mutex_),
        finished_(that.finished_) {
    that.finished_ = true;
  }

  // Not move-assignable
  UniqueLock& operator=(UniqueLock&&) = delete;

  void Lock() {
    // mutex should be unlocked
    if (finished_) {
      finished_ = false;
      mutex_->lock();
    }
  }

  void Unlock() {
    if (!finished_) {
      finished_ = true;
      mutex_->unlock();
    }
  }

  // BasicLockable

  void lock() {  // NOLINT
    Lock();
  }

  void unlock() {  // NOLINT
    Unlock();
  }

  ~UniqueLock() {
    if (!finished_) {
      mutex_->unlock();
    }
  }

 private:
  Mutex* mutex_;
  bool finished_{false};
};

}  // namespace weave::threads::blocking::stdlike
