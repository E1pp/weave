#pragma once

#include <twist/ed/stdlike/atomic.hpp>

#include <cstdlib>
#include <span>
#include <vector>

namespace weave::threads::hazard {

struct ThreadState {
  using AtomicPtr = twist::ed::stdlike::atomic<void*>;

  struct Erased {
    using Deleter = void (*)(void*);

    template <typename T>
    explicit Erased(T* ptr)
        : ptr_(reinterpret_cast<void*>(ptr)),
          deleter_([](void* ptr) {
            delete reinterpret_cast<T*>(ptr);
          }) {
    }

    bool TryDelete() {
      if (should_delete_) {
        deleter_(ptr_);

        return true;
      }

      return false;
    }

    void* ptr_;
    Deleter deleter_;
    bool should_delete_{true};
  };

  explicit ThreadState(size_t num_hazards) {
    auto allocator = std::allocator<twist::ed::stdlike::atomic<void*>>();
    auto* local_hazards = allocator.allocate(num_hazards);

    for (size_t i = 0; i < num_hazards; i++) {
      std::construct_at(local_hazards + i, nullptr);
    }

    hazards_ = std::span{local_hazards, num_hazards};
  }

  template <typename T>
  void Retire(T* ptr) {
    retirees_.push_back(Erased(ptr));

    if (retirees_.size() >= hazards_.size() * kMagicNumber) {
      Collect();
    }
  }

  void Collect();

  std::vector<Erased> retirees_;
  std::span<AtomicPtr> hazards_;

  ThreadState* next_ = nullptr;

  static inline const size_t kMagicNumber = 2;
};

}  // namespace weave::threads::hazard
