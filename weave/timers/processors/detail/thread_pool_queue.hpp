#pragma once

#include <weave/timers/processor.hpp>

#include <weave/threads/blocking/spinlock.hpp>
#include <weave/threads/blocking/stdlike/mutex.hpp>

#include <algorithm>
#include <chrono>
#include <optional>
#include <vector>

namespace weave::timers::detail {

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
      return deadline_ > that.deadline_;
    }

   public:
    TimePoint deadline_;
    ITimer* timer_;
  };

 public:
  void Push(ITimer* timer) {
    Guard locker(spinlock_);

    if(timer->CancelToken().CancelRequested()){
      cancelled_.push_back(TimerNode{GetDeadline(timer->GetDelay()), timer});
    } else {
      heap_.push_back(TimerNode{GetDeadline(timer->GetDelay()), timer});
      std::push_heap(heap_.begin(), heap_.end());
    }
  }

  std::pair<executors::Task*, std::optional<Millis>> GrabReadyTimers() {
    auto now = SteadyClock::now();

    Guard locker(spinlock_);

    ITimer* head = nullptr;
    std::optional<Millis> until_next = std::nullopt;

   // Take all timers_ from cancelled_

    while(!cancelled_.empty()){
      auto next = cancelled_.back();
      cancelled_.pop_back();

      next.timer_->next_ = head;
      head = next.timer_;
    }

    // Take expired timers from heap_

    while (!heap_.empty()) {
      auto& next = heap_.front();

      if (!(next.timer_->CancelToken().CancelRequested()) && next.deadline_ > now) {
        until_next.emplace(ToMillis(next.deadline_ - now));
        break;
      }

      next.timer_->next_ = head;
      head = next.timer_;

      std::pop_heap(heap_.begin(), heap_.end());
      heap_.pop_back();
    }

    return std::make_pair(head, until_next);
  }

  executors::Task* TakeAll() {
    executors::Task* head = nullptr;

    while (!cancelled_.empty()) {
      auto& next = cancelled_.back();

      next.timer_->next_ = head;
      head = next.timer_;

      cancelled_.pop_back();
    }

    while (!heap_.empty()) {
      auto& next = heap_.front();

      next.timer_->next_ = head;
      head = next.timer_;

      std::pop_heap(heap_.begin(), heap_.end());
      heap_.pop_back();
    }

    return head;
  }

  void ScanForCancelled(){
    Guard locker(spinlock_);

    if(heap_.empty()){
      return;
    }

    auto new_end = std::partition(heap_.begin(), heap_.end(), [](TimerNode node){
      return !node.timer_->CancelToken().CancelRequested();
    });

    cancelled_.insert(cancelled_.end(), new_end, heap_.end());

    heap_.erase(new_end, heap_.end());

    std::make_heap(heap_.begin(), heap_.end());
  }

 private:
  static TimePoint GetDeadline(Millis delay) {
    return SteadyClock::now() + delay;
  }

 private:
  weave::threads::blocking::SpinLock spinlock_{};

  std::vector<TimerNode> heap_{};
  std::vector<TimerNode> cancelled_{};
};

}  // namespace weave::timers
