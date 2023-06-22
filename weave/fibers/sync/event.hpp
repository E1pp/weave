#pragma once

#include <weave/fibers/sched/suspend.hpp>
#include <weave/fibers/sync/waiters.hpp>

namespace weave::fibers {

// One-shot

class Event {
 public:
  using Node = SimpleNode;

  void Wait() {
    if (stack_.load(std::memory_order::acquire) != fired) {
      SimpleWaiter waiter;

      auto event_awaiter = [&](FiberHandle handle) {
        waiter.SetHandle(handle);
        Node* new_node = &waiter;
        new_node->next_ = stack_.load(std::memory_order::acquire);

        do {
          if (new_node->next_ == fired) {
            return handle;
          }

        } while (!stack_.compare_exchange_weak(new_node->next_, new_node,
                                               std::memory_order::release,
                                               std::memory_order::acquire));

        return FiberHandle::Invalid();
      };

      Suspend(event_awaiter);
    }
  }

  void Fire() {
    Node* wake_list = stack_.exchange(fired, std::memory_order::acq_rel);

    if (wake_list != nullptr) {
      while (wake_list->next_ != nullptr) {
        auto tmp = wake_list->AsItem();
        wake_list = wake_list->next_;
        tmp->Schedule();
      }
      wake_list->AsItem()->Schedule();
    }
  }

 private:
  // Lock push-popall stack
  twist::ed::stdlike::atomic<Node*> stack_{nullptr};

  static inline Node* fired{fullptr};
  static inline Node* not_fired{nullptr};
};

// MO proof here:
// a) Wait:
// a.1) Load1: reading fired implies access to the critical section -> acquire.
// a.2) Load2: same as Load1 since we can exit CAS loop before any CAS op ->
// acquire. a.3) CASWeak: fail can write fired into new_node->next_ which causes
// Load2 scenario -> acquire. success never implies entrance to the crit section
// (we simply go to sleep) -> no need for acquire. success gives a valid node
// for consumer -> needs to be in hb with it -> release.
// b) Fire:
// b.1) Exchange:
// grabs a valid node to access it -> must have someone in hb with smth ->
// acquire sets ptr to fired which leads Wait callers to enter crit section ->
// must be in hb with them -> release

}  // namespace weave::fibers
