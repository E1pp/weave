#pragma once

#include <cstdint>

namespace weave::support {

// pointer or 32-bit value packed into one number
// This class heavily relies on pointer types
// having alignment greater than one
// since 1st bit is used for logic
class Word {
 public:
  // Fabrication methods

  static Word Value(uint32_t value) {
    return Word(1 | value << 1);
  }

  template <typename T>
  static Word Pointer(T* ptr) {
    static_assert(alignof(T) > 1);

    return Word(reinterpret_cast<uintptr_t>(ptr));
  }

  // Interpretation

  uint32_t AsValue() {
    return num_ >> 1;
  }

  template <typename T>
  T* AsPointerTo() {
    static_assert(alignof(T) > 1);

    return reinterpret_cast<T*>(num_);
  }

  // State checking

  bool IsValue() const {
    return static_cast<bool>(num_ & 1);
  }

  bool IsPointer() const {
    return !IsValue();
  }

  // Copyable

  Word(const Word&) = default;
  Word& operator=(const Word&) = default;

  // Comparable

  friend bool operator==(const Word& first, const Word& second) {
    return first.num_ == second.num_;
  }

  friend bool operator!=(const Word& first, const Word& second) {
    return !operator==(first, second);
  }

 private:
  // Use Fabrication methods
  explicit Word(uintptr_t num)
      : num_(num) {
  }

 private:
  uintptr_t num_;
};

}  // namespace weave::support