#pragma once

#include <weave/threads/lockfree/hazard/manager.hpp>

#include <twist/ed/stdlike/atomic.hpp>

#include <optional>

// Michael-Scott unbounded MPMC lock-free queue

namespace weave::threads::lockfree {

template <typename T>
class LockFreeQueue {
  struct Node {
    Node() = default;

    explicit Node(T item)
        : value_(std::move(item)) {
    }

    std::optional<T> value_;
    twist::ed::stdlike::atomic<Node*> next_{nullptr};
  };

 public:
  explicit LockFreeQueue(hazard::Manager* gc)
      : gc_(gc) {
    SetupQueue();
  }

  LockFreeQueue()
      : LockFreeQueue(hazard::Manager::Get()) {
  }

  void Push(T item) {
    auto mutator = gc_->MakeMutator(1);

    Node* new_node = new Node(std::move(item));
    Node* curr_tail;

    while (true) {
      curr_tail = mutator.Protect(0, tail_);

      if (curr_tail->next_.load() != nullptr) {
        // Concurrent Push

        tail_.compare_exchange_weak(curr_tail, curr_tail->next_.load());
        continue;
      }

      Node* null_ptr = nullptr;
      if (curr_tail->next_.compare_exchange_weak(null_ptr, new_node)) {
        break;
      }
    }

    tail_.compare_exchange_strong(curr_tail, new_node);
  }

  std::optional<T> TryPop() {
    auto mutator = gc_->MakeMutator(2);

    while (true) {
      Node* curr_head = mutator.Protect(0, head_);

      if (curr_head->next_.load() == nullptr) {
        return std::nullopt;
      }

      // Node* curr_tail = tail_.load();
      Node* curr_tail = mutator.Protect(1, tail_);
      if (curr_tail == curr_head) {
        tail_.compare_exchange_weak(curr_tail, curr_tail->next_.load());
      }

      Node* next = mutator.Protect(1, curr_head->next_);
      if (head_.compare_exchange_weak(curr_head, next)) {
        auto ret = std::move(next->value_);

        mutator.Retire(curr_head);
        return ret;
      }
    }
  }

  ~LockFreeQueue() {
    Node* head = head_.load();

    while (head != nullptr) {
      Node* tmp = head;
      head = head->next_.load();
      delete tmp;
    }
  }

 private:
  void SetupQueue() {
    auto* dummy = new Node();
    head_.store(dummy);
    tail_.store(dummy);
  }

 private:
  hazard::Manager* gc_;

  twist::ed::stdlike::atomic<Node*> head_{nullptr};
  twist::ed::stdlike::atomic<Node*> tail_{nullptr};
};

}  // namespace weave::threads::lockfree