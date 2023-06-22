#pragma once

#include <twist/ed/stdlike/atomic.hpp>

#include <wheels/intrusive/list.hpp>

#include <concepts>

namespace weave::threads::lockfree {

template <typename T>
class PushPopAllStack;

template <typename Function, typename T>
concept CASLoopCompaitable = std::is_invocable_r_v < bool,
        Function, wheels::IntrusiveListNode<T>
*&, wheels::IntrusiveListNode<T>*&, wheels::IntrusiveListNode<T>* > ;

template <typename T>
class PushPopAllStack {
 public:
  using Node = wheels::IntrusiveListNode<T>;

  Node* Push(Node* new_node, CASLoopCompaitable<T> auto& loop_logic,
             std::memory_order mo_succ = std::memory_order::seq_cst,
             std::memory_order mo_fail = std::memory_order::seq_cst) {
    Node* expected = top_.load(std::memory_order::acquire);
    Node* desired = nullptr;

    do {
      if (loop_logic(expected, desired, new_node)) {
        break;
      }

    } while (!top_.compare_exchange_weak(expected, desired, mo_succ, mo_fail));

    return expected;
  }

  bool CASStrong(Node* expected_val, Node* desired,
                 std::memory_order mo_succ = std::memory_order::seq_cst,
                 std::memory_order mo_fail = std::memory_order::relaxed) {
    Node* xpctd_cpy = expected_val;
    Node* dsrd_cpy = desired;
    return top_.compare_exchange_strong(xpctd_cpy, dsrd_cpy, mo_succ, mo_fail);
  }

  Node* StealStack(Node* replacement,
                   std::memory_order mo = std::memory_order::seq_cst) {
    Node* rpl_cpy = replacement;
    return top_.exchange(rpl_cpy, mo);
  }

  Node* Load(std::memory_order mo = std::memory_order::seq_cst) {
    return top_.load(mo);
  }

  static bool SimpleCASLoop(Node*& expected, Node*& desired, Node* new_node) {
    if (desired != new_node) {
      desired = new_node;
    }

    desired->next_ = expected;

    return false;
  }

 private:
  twist::ed::stdlike::atomic<Node*> top_{nullptr};
};

}  // namespace weave::threads::lockfree