#pragma once

#include <optional>
#include <vector>

namespace weave::support {

template <typename T>
class CyclicBuffer {
 public:
  explicit CyclicBuffer(size_t capacity)
      : capacity_(capacity),
        buffer_(capacity) {
  }

  bool TryPush(T val) {
    WHEELS_ASSERT(capacity_ == buffer_.size(), "Broken Invariant");
    if (IsFull()) {
      // buffer is full
      return false;
    }

    buffer_.at((tail_++) % capacity_).emplace(std::move(val));
    return true;
  }

  std::optional<T> TryPop() {
    WHEELS_ASSERT(capacity_ == buffer_.size(), "Broken Invariant");
    if (IsEmpty()) {
      // buffer is empty
      return std::nullopt;
    }

    return std::move(*buffer_.at((head_++) % capacity_));
  }

  bool IsEmpty() const {
    return head_ == tail_;
  }

  bool IsFull() const {
    return tail_ - head_ == capacity_;
  }

 private:
  [[maybe_unused]] const size_t capacity_;
  std::vector<std::optional<T>> buffer_;
  size_t head_{0};
  size_t tail_{0};
};

}  // namespace weave::support