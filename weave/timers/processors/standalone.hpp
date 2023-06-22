#pragma once

#include <weave/timers/processor.hpp>

#include <weave/threads/lockfull/spinlock.hpp>
#include <weave/threads/lockfull/stdlike/mutex.hpp>

#include <twist/ed/stdlike/thread.hpp>

#include <chrono>
#include <optional>
#include <queue>

namespace weave::timers {

using namespace std::chrono_literals;

// Takes up one thread to process timers

// Currently uses spinlock + priority queue

class StandaloneProcessor : public IProcessor {
  using Guard = weave::threads::lockfull::stdlike::LockGuard<
      weave::threads::lockfull::SpinLock>;
  using SteadyClock = std::chrono::steady_clock;
  using TimePoint = SteadyClock::time_point;

  // Highest priority <-> closest deadline
  struct PriorityQueueNode {
   public:
    explicit PriorityQueueNode(TimePoint dl, ITimer* t)
        : deadline_(dl),
          timer_(t) {
    }

    bool operator<(const PriorityQueueNode& that) const {
      return deadline_ > that.deadline_;
    }

   public:
    TimePoint deadline_;
    ITimer* timer_;
  };

 public:
  StandaloneProcessor()
      : worker_([this] {
          WorkerLoop();
        }) {
  }

  // IProcessor
  void AddTimer(ITimer* timer) override {
    Push(timer);

    TryWakeWorker();
  }

  Delay DelayFromThis(Millis ms) override {
    return Delay{ms, *this};
  }

  ~StandaloneProcessor() override {
    Stop();
  }

 private:
  void WorkerLoop() {
    uint32_t old;

    while (!stop_requested_.load(std::memory_order::relaxed)) {
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

  void Push(ITimer* timer) {
    Guard locker(spinlock_);
    queue_.push(PriorityQueueNode(GetDeadline(timer->GetDelay()), timer));
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
    while (!queue_.empty()) {
      auto top = queue_.top();
      queue_.pop();

      top.timer_->Callback();
    }
  }

  void Stop() {
    stop_requested_.store(true, std::memory_order::relaxed);
    WakeWorker();

    worker_.join();
  }

  static TimePoint GetDeadline(Millis delay) {
    return SteadyClock::now() + delay;
  }

  // nullopt if queue was depleted
  std::optional<Millis> PollQueue() {
    auto [timer_list, ms_until_inactive] = GrabReadyTimers();

    while (timer_list != nullptr) {
      ITimer* next = static_cast<ITimer*>(timer_list->next_);

      timer_list->Callback();

      timer_list = next;
    }

    return ms_until_inactive;
  }

  std::pair<ITimer*, std::optional<Millis>> GrabReadyTimers() {
    auto now = SteadyClock::now();

    Guard locker(spinlock_);

    ITimer* head = nullptr;
    std::optional<Millis> until_next = std::nullopt;

    while (!queue_.empty()) {
      auto& next = queue_.top();

      if (next.deadline_ > now) {
        until_next.emplace(ToMillis(next.deadline_ - now));
        break;
      }

      next.timer_->next_ = head;
      head = next.timer_;

      queue_.pop();
    }

    return std::make_pair(head, until_next);
  }

 private:
  twist::ed::stdlike::atomic<bool> stop_requested_{false};
  twist::ed::stdlike::atomic<uint32_t> wakeups_{0};
  twist::ed::stdlike::atomic<bool> idle_{false};

  weave::threads::lockfull::SpinLock spinlock_{};
  std::priority_queue<PriorityQueueNode> queue_{};

  // NB : Worker created last to have every
  // other constructor in hb with it
  twist::ed::stdlike::thread worker_;
};

}  // namespace weave::timers