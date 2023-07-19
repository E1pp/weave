#pragma once

#include <weave/threads/lockfree/count_and_flags.hpp>

#include <wheels/core/assert.hpp>

#include <vector>

namespace weave::threads::lockfree {

template <typename T>
struct TaggedNode {
  T* AsItem() {
    return static_cast<T*>(this);
  }

  int position_{-1};
};

// Supported methods:
// TryPush(TaggedNode*)
// Pop(TaggedNode*)
// SealBuffer()
// IsSealed()

template <typename T>
class TaggedBuffer {
  static_assert(std::is_base_of_v<TaggedNode<T>, T>);

  using Node = TaggedNode<T>;
  using State = CountAndFlags<1>;
  static inline Node* fullptr =
      reinterpret_cast<Node*>(std::numeric_limits<uintptr_t>::max());

  struct Slot {
    twist::ed::stdlike::atomic<Node*> ptr_{nullptr};
  };

 public:
  explicit TaggedBuffer(size_t capacity)
      : buffer_(capacity) {
  }

  bool TryPush(Node* node) {
    WHEELS_VERIFY(node->position_ == -1, "Already pushed!");

    uint64_t observed = current_size_.load(std::memory_order::acquire);

    do {
      if (State::ReadFlag<1>(observed)) {
        // Buffer is sealed
        return false;
      }

      WHEELS_VERIFY(State::ReadCount(observed) < buffer_.size(),
                    "Buffer is full!");

      node->position_ = State::ReadCount(observed);

    } while (!current_size_.compare_exchange_weak(observed, observed + 2,
                                                  std::memory_order::relaxed,
                                                  std::memory_order::acquire));

    Node* null_ptr = nullptr;

    // false if read fullptr
    return buffer_[State::ReadCount(observed)].ptr_.compare_exchange_strong(
        null_ptr, node, std::memory_order::release, std::memory_order::acquire);
  }

  // Postcondition : node on the given position is fullptr
  // true if we got it first
  bool TryPop(Node* node) {
    WHEELS_VERIFY(node->position_ != -1, "Not pushed yet!");

    return buffer_[node->position_].ptr_.exchange(
               fullptr, std::memory_order::relaxed) != fullptr;
  }

  // One-shot
  void SealBuffer(void (*processor)(T*)) {
    uint64_t observed = current_size_.fetch_add(1, std::memory_order::release);
    uint32_t size = State::ReadCount(observed);

    for (size_t index = 0; index < size; index++) {
      Node* item =
          buffer_[index].ptr_.exchange(fullptr, std::memory_order::acquire);

      if (item == fullptr || item == nullptr) {
        // we have either read a removed item or an item which is was not ready
        // skip in both cases
        continue;
      }

      processor(item->AsItem());
    }
  }

  bool IsSealed() {
    return State::ReadFlag<1>(current_size_.load(std::memory_order::acquire));
  }

 private:
  std::vector<Slot> buffer_;
  twist::ed::stdlike::atomic<uint64_t> current_size_{0};
};

}  // namespace weave::threads::lockfree