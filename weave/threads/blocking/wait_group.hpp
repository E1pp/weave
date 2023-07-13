#pragma once

#include <twist/ed/stdlike/atomic.hpp>
#include <twist/ed/wait/sys.hpp>

#include <wheels/core/assert.hpp>

#include <cstdlib>

#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace weave::threads::blocking {

class WG;
class WaitGroup1;

using WaitGroup = WaitGroup1;

class WaitGroup1 {
  enum State : uint32_t { WorkIsNotDone = 0, WorkIsDone = 1 };

 public:
  // += count
  void Add(size_t count) {
    // clown case
    if (count == 0) {
      return;
    }

    // add work
    if (counter_.fetch_add(count, std::memory_order::relaxed) == 0) {
      state_.store(State::WorkIsNotDone, std::memory_order::relaxed);
    }
  }

  // =- 1
  void Done() {
    if (counter_.fetch_sub(1, std::memory_order::acq_rel) == 1) {
      // we are the last one and need to wake potential waiter
      auto wake_key = twist::ed::futex::PrepareWake(state_);

      state_.store(State::WorkIsDone, std::memory_order::release);

      twist::ed::futex::WakeAll(wake_key);
    }
  }

  void Wait() {
    twist::ed::futex::Wait(state_, State::WorkIsNotDone,
                           std::memory_order::acquire);
  }

 private:
  twist::ed::stdlike::atomic<uint64_t> counter_{0};
  twist::ed::stdlike::atomic<uint32_t> state_{State::WorkIsDone};
};

// MO proof:
// 1) Add doesn't cause entry into critical section so doesn't need hb
// connection 2) Every Done must be in hb with wait they release. Last Done is
// in hb wait via state_ SW. every non-final Done are structured into a hb chain
// which terminates on the last Done via acq_rel fetch_sub.

///////////////////////////////////////////////////////////////////////////////////

// Experimental reusable waitgroup which allows concurrent Add and Done
// Platform dependant and also has IDB issues, so don't use that

class WG {
 public:
  void Add(uint32_t count) {
    if (count == 0) {
      return;
    }

    uint64_t obs = state_.load();
    uint64_t repl;

    do {
      repl = AddCount(obs, count);

    } while (!state_.compare_exchange_weak(obs, repl));
  }

  void Done() {
    Done(1);
  }

  void Done(uint32_t count) {
    if (count == 0) {
      return;
    }

    auto prepare_wake = twist::ed::futex::PrepareWake(AtomicForFutex());

    uint64_t obs = state_.load();
    uint64_t repl;

    do {
      repl = ReduceCount(obs, count);

    } while (!state_.compare_exchange_weak(obs, repl));

    if (obs == (static_cast<uint64_t>(count) << 32)) {
      twist::ed::futex::WakeAll(prepare_wake);
    }
  }

  void Wait() {
    auto addr1 = reinterpret_cast<uint32_t*>(&state_);

    while (ReadFirstBits(state_.load()) == kNotEmpty) {
      syscall(SYS_futex, addr1, FUTEX_WAIT_PRIVATE, kNotEmpty, nullptr, nullptr,
              0);
    }
  }

 private:
  twist::ed::stdlike::atomic<uint32_t>& AtomicForFutex() {
    return *reinterpret_cast<twist::ed::stdlike::atomic<uint32_t>*>(&state_);
  }

  // Helpers with updating info

  uint64_t AddCount(uint64_t num, uint32_t count) {
    WHEELS_VERIFY(count > 0, "Don't be a clown");

    if (num == kEmpty) {
      return static_cast<uint64_t>(count) << 32;
    } else {
      return num + (static_cast<uint64_t>(count) << 32);
    }
  }

  uint64_t ReduceCount(uint64_t num, uint32_t count) {
    WHEELS_VERIFY(count > 0, "Don't be a clown");

    if (num == (static_cast<uint64_t>(count) << 32)) {
      return kEmpty;
    } else {
      return num - (static_cast<uint64_t>(count) << 32);
    }
  }

  uint32_t ReadFirstBits(uint64_t num) {
    return num & ((static_cast<uint64_t>(1) << 32) - 1);
  }

 private:
  twist::ed::stdlike::atomic<uint64_t> state_{kEmpty};
  twist::ed::stdlike::atomic<uint32_t> dummy_{0};
  static constexpr uint64_t kEmpty = 1;
  static constexpr uint64_t kNotEmpty = 0;
};

}  // namespace weave::threads::blocking