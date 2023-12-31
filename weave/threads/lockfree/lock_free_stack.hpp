#pragma once

#include <weave/threads/lockfree/hazard/manager.hpp>

#include <twist/ed/stdlike/atomic.hpp>

#include <optional>

namespace weave::threads::lockfree {

// Treiber unbounded MPMC lock-free stack

template <typename T>
class LockFreeStack {
  using Manager = hazard::Manager;
  struct Node {
    T item;
    Node* next{nullptr};
  };

 public:
  explicit LockFreeStack(Manager* gc)
      : gc_(gc) {
  }

  LockFreeStack()
      : LockFreeStack(Manager::Get()) {
  }

  void Push(T item) {
    Node* new_node = new Node{std::move(item)};
    new_node->next = top_.load();

    while (!top_.compare_exchange_weak(new_node->next, new_node)) {
      ;
    }
  }

  std::optional<T> TryPop() {
    auto mutator = gc_->MakeMutator();

    while (true) {
      Node* curr_top = mutator.Protect(0, top_);

      if (curr_top == nullptr) {
        return std::nullopt;
      }

      if (top_.compare_exchange_weak(curr_top, curr_top->next)) {
        T item = std::move(curr_top->item);
        mutator.Retire(curr_top);
        return item;
      }
    }
  }

  ~LockFreeStack() {
    Node* curr = top_.load();
    while (curr != nullptr) {
      Node* next = curr->next;
      delete curr;
      curr = next;
    }
  }

 private:
  Manager* gc_;
  twist::ed::stdlike::atomic<Node*> top_{nullptr};
};

}  // namespace weave::threads::lockfree