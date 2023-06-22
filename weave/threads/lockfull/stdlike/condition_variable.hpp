#pragma once

#include <twist/ed/stdlike/atomic.hpp>
#include <twist/ed/wait/sys.hpp>

#include <cstdint>

namespace weave::threads::lockfull::stdlike {
class CondVar {
  using Time = uint32_t;

 public:
  CondVar() = default;

  // Pinned
  CondVar(const CondVar&) = delete;
  CondVar& operator=(const CondVar&) = delete;

  CondVar(CondVar&&) = delete;
  CondVar& operator=(CondVar&&) = delete;

  // Mutex - BasicLockable
  // https://en.cppreference.com/w/cpp/named_req/BasicLockable
  template <class Mutex>
  void Wait(Mutex& mutex) {
    Time entry_time = timer_.load();
    mutex.unlock();

    twist::ed::Wait(timer_, entry_time);

    mutex.lock();
  }

  void NotifyOne() {
    Notify(twist::ed::WakeOne);
  }

  void NotifyAll() {
    Notify(twist::ed::WakeAll);
  }

 private:
  template <typename T>
  void Notify(void (*waker)(T)) {
    auto wake_key = twist::ed::PrepareWake(timer_);

    timer_.fetch_add(1);
    waker(wake_key);
  }

 private:
  twist::ed::stdlike::atomic<Time> timer_{0};
};

}  // namespace weave::threads::lockfull::stdlike
