#pragma once

#include <weave/timers/processor.hpp>

#include <weave/timers/processors/thread_pool_queue.hpp>

#include <twist/ed/stdlike/thread.hpp>

#include <chrono>
#include <optional>
#include <queue>

namespace weave::timers {

using namespace std::chrono_literals;

// Takes up one thread to process timers

class StandaloneProcessor : public IProcessor {
 public:
  StandaloneProcessor()
      : worker_([this] {
          WorkerLoop();
        }) {
  }

  // IProcessor
  void AddTimer(ITimer* timer) override {
    queue_.Push(timer);

    TryWakeWorker();
  }

  void CancelTimer(ITimer* timer) override {
    queue_.Cancel(timer);
    TryWakeWorker();
  }

  ~StandaloneProcessor() override {
    Stop();
  }

 private:
  void WorkerLoop() {
    uint32_t old;

    while (!stop_requested_.load(std::memory_order::acquire)) {
      old = wakeups_.load();

      auto until_next_deadline = PollQueue();

      idle_.store(true, std::memory_order::seq_cst);

      // Update sleep duration
      until_next_deadline = PollQueue();
      // if PollQueue in hb with Push then idle_.load which is seq after Push
      // must be in hb with idle_.store thus seq_cst on store-load here

      if (until_next_deadline) {
        Millis roundup = std::max(1ms, *until_next_deadline);
        twist::ed::futex::WaitTimed(wakeups_, old,
                                    roundup /*, std::memory_order::relaxed*/);
      } else {
        twist::ed::futex::Wait(wakeups_, old, std::memory_order::relaxed);
      }

      idle_.store(false, std::memory_order::relaxed);
    }

    ClearQueue();
  }

  // nullopt if queue was depleted
  std::optional<Millis> PollQueue() {
    auto [timer_list, ms_until_inactive] = queue_.GrabReadyTimers();

    while (timer_list != nullptr) {
      auto* next = static_cast<executors::Task*>(timer_list->next_);

      timer_list->Run();

      timer_list = next;
    }

    return ms_until_inactive;
  }

  void TryWakeWorker() {
    if (idle_.load(std::memory_order::seq_cst)) {
      WakeWorker();
    }
  }

  void WakeWorker() {
    auto key = twist::ed::futex::PrepareWake(wakeups_);
    wakeups_.fetch_add(1, std::memory_order::relaxed);
    twist::ed::futex::WakeOne(key);
  }

  // Only called during the destructor call
  // thus every push is in hb with us without spinlock
  void ClearQueue() {
    auto* head = queue_.TakeAll();

    while (head != nullptr) {
      auto* next = static_cast<executors::Task*>(head->next_);

      static_cast<ITimer*>(head)->WasCancelled();

      head->Run();

      head = next;
    }
  }

  void Stop() {
    stop_requested_.store(true, std::memory_order::release);
    WakeWorker();

    worker_.join();
  }

 private:
  twist::ed::stdlike::atomic<bool> stop_requested_{false};
  twist::ed::stdlike::atomic<uint32_t> wakeups_{0};
  twist::ed::stdlike::atomic<bool> idle_{false};

  weave::timers::TimersQueue queue_{};

  // NB : Worker created last to have every
  // other constructor in hb with it
  twist::ed::stdlike::thread worker_;
};

}  // namespace weave::timers