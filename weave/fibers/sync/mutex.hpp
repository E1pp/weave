#pragma once

#include <weave/fibers/sched/suspend.hpp>
#include <weave/fibers/sync/waiters.hpp>

#include <wheels/core/assert.hpp>

namespace weave::fibers {

class Mutex {
 public:
  using Node = SimpleNode;

  void Lock() {
    Node* is_unlocked_cpy = is_unlocked;
    if (stack_.compare_exchange_strong(is_unlocked_cpy, is_locked,
                                       std::memory_order::acquire,
                                       std::memory_order::relaxed)) {
      // We try to leave early
      return;
    }

    // we didn't make it and have to enqueue
    SimpleWaiter waiter{};
    auto mutex_awaiter = [&](FiberHandle handle) {
      waiter.SetHandle(handle);

      // we can possibly write observed = in_unlock_cpy since it contains needed
      // data
      Node* observed = stack_.load(std::memory_order::relaxed);
      Node* new_node;

      do {
        if (observed != is_unlocked) {
          new_node = &waiter;
          new_node->next_ = observed;

        } else {
          new_node = is_locked;
        }

      } while (!stack_.compare_exchange_weak(observed, new_node,
                                             std::memory_order::acq_rel,
                                             std::memory_order::relaxed));

      return observed == is_unlocked ? handle : FiberHandle::Invalid();
    };

    Suspend(mutex_awaiter);
  }

  // Disable symm transfer if called due to cancellation?
  void Unlock() {
    if (first_in_queue_ == nullptr) {
      // we need to grab more from lock-free stack to restock
      Node* is_locked_cpy = is_locked;
      if (stack_.compare_exchange_strong(is_locked_cpy, is_unlocked,
                                         std::memory_order::release,
                                         std::memory_order::relaxed)) {
        // Unlock mutex and leave early
        return;
      }

      RestockQueue();
    }

    auto next_owner = first_in_queue_->AsItem();
    first_in_queue_ = first_in_queue_->prev_;

    // schedule next owner after us
    next_owner->Schedule(executors::SchedulerHint::Next);

    // suspend with rescheduling yourself
    auto unlock_awaiter = [](FiberHandle handle) {
      // schedule self in local queue
      handle.Schedule();

      // return invalid because we want FiberRunner to end
      return FiberHandle::Invalid();
    };

    Suspend(unlock_awaiter);
  }

  // BasicLockable

  void lock() {  // NOLINT
    Lock();
  }

  void unlock() {  // NOLINT
    Unlock();
  }

 private:
  // requires non-empty stack
  void RestockQueue() {
    Node* stolen_stack = stack_.exchange(
        is_locked, std::memory_order::acquire);  // steal the stack
    // stolen_stack == valid Node*

    // something was stolen and we can exit the loop
    stolen_stack->prev_ = nullptr;

    while (stolen_stack->next_ != is_locked) {
      stolen_stack->next_->prev_ = stolen_stack;
      stolen_stack = stolen_stack->next_;
    }

    // right here stolen_stack is valid but stolen_stack->next_ is not
    stolen_stack->next_ = nullptr;
    first_in_queue_ = stolen_stack;
  }

 private:
  // Lock push-popall stack
  twist::ed::stdlike::atomic<Node*> stack_{nullptr};

  // Unlock stack for fairness guarantee
  Node* first_in_queue_{nullptr};

  static inline Node* is_unlocked{nullptr};
  static inline Node* is_locked{fullptr};
};

// MO proof here:
// a) Lock:
// a.1) CASStrong: fail writes into tmp object which is discarded instantly ->
// relaxed. success is followed by crit zone -> must have smth in hb with ->
// acquire. locking doesn't do shit -> no release. a.2) Load: first observation
// is always followed by CASWeak which ensures every hb relation on it's own
// anyway -> relaxed. a.3) CASWeak: fail re-enters CASWeak -> relaxed. success
// with inserting is_locked implies access to crit section -> must have smth in
// hb with -> acquire. success with inserting valid node -> should be in hb with
// Lock who wakes this node -> release. b) Unlock: b.1) CASStrong: fail writes
// into tmp -> relaxed. success let's another fibers into crit section -> must
// be in hb with someone -> release. b.2) Restock Exchange: read data is
// accessed right after -> must have smth in hb relation -> acquire.

}  // namespace weave::fibers
