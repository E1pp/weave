#pragma once

#include <weave/threads/lockfree/hazard/thread_state.hpp>

#include <wheels/core/assert.hpp>

namespace weave::threads::hazard {

class Mutator {
  template <typename T>
  using AtomicPtr = twist::ed::stdlike::atomic<T*>;

 public:
  explicit Mutator(ThreadState* state)
      : state_(state) {
  }

  template <typename T>
  T* Protect(size_t index, AtomicPtr<T>& ptr) {
    // remove old ptr from hazard list
    state_->hazards_[index].store(nullptr);

    T* current = ptr.load();
    T* old;

    do {
      old = current;

      state_->hazards_[index].store(current);

      current = ptr.load();

    } while (old != current);

    return current;
  }

  template <typename T>
  void Retire(T* ptr) {
    // remove position from hazard
    for (auto& hazard : state_->hazards_) {
      if (hazard.load() == ptr) {
        hazard.store(nullptr);
      }
    }

    // add ptr to the retire list
    state_->Retire(ptr);
  }

  ~Mutator() {
    // reset hazards
    for (auto& hazard : state_->hazards_) {
      // don't write null over null to reduce mesi communication
      if (hazard.load() != nullptr) {
        hazard.store(nullptr);
      }
    }
  }

 private:
  ThreadState* state_;
};

}  // namespace weave::threads::hazard
