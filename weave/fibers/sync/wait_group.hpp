#pragma once

#include <weave/fibers/sched/suspend.hpp>
#include <weave/fibers/sync/waiters.hpp>

namespace weave::fibers {

// https://gobyexample.com/waitgroups

// all HB relations are either guaranteed by stack structure (top_ r, w, rmw
// operations) or by the user (counter_ rmw's)

// test new Add
class WaitGroup {
 public:
  using Node = SimpleNode;

  void Add(size_t count) {
    if (counter_.fetch_add(count, std::memory_order::relaxed) == 0) {
      // reset the state
      Node* wid_cpy = work_is_done;
      stack_.compare_exchange_strong(wid_cpy, work_is_not_done_yet,
                                     std::memory_order::relaxed,
                                     std::memory_order::relaxed);
    }
  }

  void Done() {
    if (counter_.fetch_sub(1, std::memory_order::acq_rel) == 1) {
      // if we have reached 0 we need to wake everyone who's sleeping rn
      Node* wake_list =
          stack_.exchange(work_is_done, std::memory_order::acq_rel);

      if (wake_list != work_is_not_done_yet) {
        while (wake_list->next_ != work_is_not_done_yet) {
          auto tmp = wake_list->AsItem();
          wake_list = wake_list->next_;
          tmp->Schedule();
        }

        wake_list->AsItem()->Schedule();
      }
    }
  }

  void Wait() {
    if (stack_.load(std::memory_order::acquire) != work_is_done) {
      SimpleWaiter waiter;
      auto wait_group_awaiter = [&](FiberHandle handle) {
        waiter.SetHandle(handle);

        Node* new_node = &waiter;
        new_node->next_ = stack_.load(std::memory_order::acquire);

        do {
          if (new_node->next_ == work_is_done) {
            return handle;
          }

        } while (!stack_.compare_exchange_weak(new_node->next_, new_node,
                                               std::memory_order::release,
                                               std::memory_order::acquire));

        return FiberHandle::Invalid();
      };

      Suspend(wait_group_awaiter);
    }
  }

 private:
  alignas(64) twist::ed::stdlike::atomic<uint64_t> counter_{0};

  // queue stuff
  twist::ed::stdlike::atomic<Node*> stack_{nullptr};

  static inline Node* work_is_not_done_yet{nullptr};
  static inline Node* work_is_done{fullptr};
};

// MO proof etc:
// 1) Add's are in hb with Done via user so we delegate all sync to Done.
// 2) non-final Done's are synced with each other via fetch_sub
// 3) final Done synced with exiting read on wait via xchg on stack_

}  // namespace weave::fibers
