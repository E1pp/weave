#pragma once

#include <algorithm>
#include <weave/timers/processor.hpp>

#include <weave/threads/blocking/spinlock.hpp>
#include <weave/threads/blocking/stdlike/mutex.hpp>

#include <chrono>
#include <optional>
#include <deque>
#include <span>

namespace weave::timers {

using namespace std::chrono_literals;

// PriorityQueue for timers to be used by scheduler which handles timers

class TimersQueue {
 private:
  using Guard = weave::threads::blocking::stdlike::LockGuard<
      weave::threads::blocking::SpinLock>;
  using SteadyClock = std::chrono::steady_clock;
  using TimePoint = SteadyClock::time_point;

  struct TimerNode {
   public:
    explicit TimerNode(TimePoint dl, ITimer* t)
        : deadline_(dl),
          timer_(t) {
    }

    bool operator<(const TimerNode& that) const {
      if (timer_->CancelToken().CancelRequested() && !that.timer_->CancelToken().CancelRequested()) {
        return false;
      }

      if (!timer_->CancelToken().CancelRequested() && that.timer_->CancelToken().CancelRequested()) {
        return true;
      }

      return deadline_ > that.deadline_;
    }

   public:
    TimePoint deadline_;
    ITimer* timer_;
  };

 public:
  void Push(ITimer* timer) {
    Guard locker(spinlock_);

    timers_.push_back(TimerNode{GetDeadline(timer->GetDelay()), timer});
    std::make_heap(timers_.begin(), timers_.end());
  }

  // // Takes timers from queue (including the ongoing ones)
  // void Grab(std::span<executors::Task*> buffer) {
  //   const size_t size =
  //       std::max<size_t>(1, std::min(buffer.size(), timers_.size() / 2));

  //   Guard locker(spinlock_);

  //   for (size_t i = 0; i < size && !timers_.empty(); i++) {
  //     buffer[i] = timers_.front().timer_;
  //     std::pop_heap(timers_.begin(), timers_.end());
  //     timers_.pop_back();
  //   }
  // }

  // void Grab(std::span<ITimer*> buffer) {
  //   const size_t size =
  //       std::max<size_t>(1, std::min(buffer.size(), timers_.size() / 2));

  //   Guard locker(spinlock_);

  //   for (size_t i = 0; i < size && !timers_.empty(); i++) {
  //     buffer[i] = timers_.front().timer_;
  //     std::pop_heap(timers_.begin(), timers_.end());
  //     timers_.pop_back();
  //   }
  // }

  std::pair<executors::Task*, bool> GrabReadyTimers() {
    auto now = SteadyClock::now();

    Guard locker(spinlock_);

    std::make_heap(timers_.begin(), timers_.end());

    ITimer* head = nullptr;
    bool is_empty = false;

    while (!timers_.empty()) {
      auto& next = timers_.front();

      if (!(next.timer_->CancelToken().CancelRequested()) && next.deadline_ > now) {
        is_empty = true;
        break;
      }

      next.timer_->next_ = head;
      head = next.timer_;

      timers_.pop_front();
      std::make_heap(timers_.begin(), timers_.end());
    }

    return std::make_pair(head, is_empty);
  }

  executors::Task* TakeAll() {
    executors::Task* head = nullptr;

    while (!timers_.empty()) {
      auto& next = timers_.front();

      next.timer_->next_ = head;
      head = next.timer_;

      timers_.pop_front();
      std::make_heap(timers_.begin(), timers_.end());
    }

    return head;
  }

 private:
  static TimePoint GetDeadline(Millis delay) {
    return SteadyClock::now() + delay;
  }

 private:
  weave::threads::blocking::SpinLock spinlock_{};
  std::deque<TimerNode> timers_{};
};

}  // namespace weave::timers
