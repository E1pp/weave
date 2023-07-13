#pragma once

#include <twist/ed/stdlike/atomic.hpp>
#include <twist/ed/wait/futex.hpp>

namespace weave::threads::blocking {

class Event {
  enum State : uint32_t { NotReady = 0, Ready = 1 };

 public:
  void Wait() {
    twist::ed::futex::Wait(ready_, State::NotReady, std::memory_order::acquire);
  }

  void Set() {
    auto wake_key = twist::ed::futex::PrepareWake(ready_);
    ready_.store(1, std::memory_order::release);
    twist::ed::futex::WakeOne(wake_key);
  }

 private:
  twist::ed::stdlike::atomic<uint32_t> ready_{State::NotReady};
};

}  // namespace weave::threads::blocking