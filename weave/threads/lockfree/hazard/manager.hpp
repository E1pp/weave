#pragma once

#include <weave/threads/lockfree/hazard/mutator.hpp>

namespace weave::threads::hazard {

// TODO: memory orders everywhere!!!
class Manager {
  friend struct ThreadState;

 public:
  static Manager* Get();

  Mutator MakeMutator(size_t n_hazards = 1);

  void Collect();

 private:
  void Push(ThreadState* state) {
    state->next_ = states_.load(std::memory_order::relaxed);
    while (!states_.compare_exchange_weak(state->next_, state,
                                          std::memory_order::release,
                                          std::memory_order::relaxed)) {
      ;  // Backoff
    }
  }

  // we assume member fields reading so
  ThreadState* StackHead() {
    return states_.load(std::memory_order::acquire);
  }

  twist::ed::stdlike::atomic<ThreadState*> states_{nullptr};
  // MO's related to states_ are the ones of push_one -- pop_all stack
};

}  // namespace weave::threads::hazard
