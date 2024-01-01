#pragma once

#include <atomic>
#include <weave/timers/processor.hpp>

#include <weave/threads/blocking/spinlock.hpp>
#include <weave/threads/blocking/stdlike/mutex.hpp>

#include <algorithm>
#include <chrono>
#include <optional>
#include <vector>
#include "weave/timers/timer.hpp"
#include "wheels/intrusive/list.hpp"

namespace weave::timers::detail {

using namespace std::chrono_literals;

// PriorityQueue for timers to be used by scheduler which handles timers

class TimersQueue {
 private:
  using SteadyClock = std::chrono::steady_clock;
  using TimePoint = SteadyClock::time_point;

 public:
  void Push(TimerBase* timer) {
    timer->deadline = GetDeadline(timer->GetDelay());
    PushToStack(timer);
  }

  void PostCancelRequest() {
    // Not Implemented :(
  }

  std::pair<wheels::IntrusiveList<TimerBase>, std::optional<Millis>> GrabReadyTimers() {
    auto now = SteadyClock::now();

    UpdateQueueState();

    wheels::IntrusiveList<TimerBase> timers = std::move(cancelled_);
    std::optional<Millis> until_next = std::nullopt;

    // Take expired timers from heap_

    while (!heap_.empty()) {
      TimerBase* next = heap_.front();

      // Timer could be cancelled after heap was generated
      if (!(next->CancelToken().CancelRequested()) && next->deadline > now) {
        until_next.emplace(ToMillis(next->deadline - now));
        break;
      }

      timers.PushBack(next);

      std::pop_heap(heap_.begin(), heap_.end(), kLess);
      heap_.pop_back();
    }

    return std::make_pair(std::move(timers), until_next);
  }

  wheels::IntrusiveList<TimerBase> TakeAll() {
    UpdateQueueState();

    wheels::IntrusiveList<TimerBase> timers = std::move(cancelled_);

    while (!heap_.empty()) {
      TimerBase* next = heap_.front();

      timers.PushBack(next);

      std::pop_heap(heap_.begin(), heap_.end(), kLess);
      heap_.pop_back();
    }

    return timers;
  }

  void UpdateQueueState() {
    if (!heap_.empty()) {
      RemoveCancelledFromHeap();
    }

    TimerBase* current = TakeFromStack();
    while (current != nullptr) {
      TimerBase* next = static_cast<TimerBase*>(current->prev_);
      current->Unlink();

      if (current->CancelToken().CancelRequested()) {
        cancelled_.PushBack(current);
      } else {
        heap_.push_back(current);
      }

      current = next;
    }

    std::make_heap(heap_.begin(), heap_.end(), kLess);
  }

  void RemoveCancelledFromHeap() {
    auto new_end = std::partition(heap_.begin(), heap_.end(), [](TimerBase* node){
      return !node->CancelToken().CancelRequested();
    });

    for (auto iter = new_end; iter != heap_.end(); ++iter) {
      cancelled_.PushBack(*iter);
    }

    heap_.erase(new_end, heap_.end());
  }

 private:
  static TimePoint GetDeadline(Millis delay) {
    return SteadyClock::now() + delay;
  }

  void PushToStack(TimerBase* new_head) {
    TimerBase* curr_head = stack_.load(std::memory_order::relaxed);

    do {
      new_head->prev_ = curr_head;
    } while(!stack_.compare_exchange_weak(curr_head, new_head, std::memory_order::release, std::memory_order::relaxed));
  }

  TimerBase* TakeFromStack() {
    return stack_.exchange(nullptr, std::memory_order::acquire);
  }

 private:
  twist::ed::stdlike::atomic<TimerBase*> stack_{nullptr};

  wheels::IntrusiveList<TimerBase> cancelled_{};
  std::vector<TimerBase*> heap_{};

  static constexpr auto kLess = [] (const TimerBase* lhs, const TimerBase* rhs) {
    return lhs->deadline > rhs->deadline;
  };
};

}  // namespace weave::timers
