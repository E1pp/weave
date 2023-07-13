#pragma once

#include <twist/ed/stdlike/atomic.hpp>

#include <type_traits>
#include <vector>

namespace weave::threads::lockfree {

template <typename T>
requires std::is_default_constructible_v<T>
class AtomicArray {
 private:
  struct Slot {
    twist::ed::stdlike::atomic<T> obj_{T{}};
  };

 public:
  explicit AtomicArray(size_t count)
      : storage_(count) {
  }

  decltype(auto) Load(size_t index,
                      std::memory_order mo = std::memory_order::seq_cst) {
    return storage_[index].obj_.load(mo);
  }

  decltype(auto) FetchAdd(size_t index, T diff,
                          std::memory_order mo = std::memory_order::seq_cst) {
    return storage_[index].obj_.fetch_add(std::move(diff), mo);
  }

  void Store(size_t index, T next,
             std::memory_order mo = std::memory_order::seq_cst) {
    storage_[index].obj_.store(std::move(next), mo);
  }

  size_t Size() const {
    return storage_.size();
  }

 private:
  std::vector<Slot> storage_;
};

template <typename T, bool Atomic>
class MaybeAtomicArray {
 public:
  explicit MaybeAtomicArray(size_t count)
      : impl_(count) {
  }

  decltype(auto) Load(size_t index, [[maybe_unused]] std::memory_order mo =
                                        std::memory_order::seq_cst) {
    return impl_[index];
  }

  decltype(auto) FetchAdd(
      size_t index, T diff,
      [[maybe_unused]] std::memory_order mo = std::memory_order::seq_cst) {
    return impl_[index] += diff;
  }

  void Store(
      size_t index, T next,
      [[maybe_unused]] std::memory_order mo = std::memory_order::seq_cst) {
    impl_[index] = std::move(next);
  }

  size_t Size() const {
    return impl_.size();
  }

 private:
  std::vector<T> impl_;
};

template <typename T>
class MaybeAtomicArray<T, true> {
 public:
  explicit MaybeAtomicArray(size_t count)
      : impl_(count) {
  }

  decltype(auto) Load(size_t index, [[maybe_unused]] std::memory_order mo =
                                        std::memory_order::seq_cst) {
    return impl_.Load(index, mo);
  }

  decltype(auto) FetchAdd(
      size_t index, T diff,
      [[maybe_unused]] std::memory_order mo = std::memory_order::seq_cst) {
    return impl_.FetchAdd(index, std::move(diff), mo);
  }

  void Store(
      size_t index, T next,
      [[maybe_unused]] std::memory_order mo = std::memory_order::seq_cst) {
    impl_.Store(index, std::move(next), mo);
  }

  size_t Size() const {
    return impl_.Size();
  }

 private:
  AtomicArray<T> impl_;
};

}  // namespace weave::threads::lockfree