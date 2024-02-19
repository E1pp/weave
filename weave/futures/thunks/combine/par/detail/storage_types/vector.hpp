#pragma once

#include <weave/futures/model/evaluation.hpp>

#include <weave/futures/thunks/detail/cancel_base.hpp>

#include <weave/support/constructor_bases.hpp>

#include <weave/result/types/unit.hpp>

#include <vector>

namespace weave::futures::thunks::detail {

///////////////////////////////////////////////////////////////////////////////////////

template <Thunk Future>
class Vector final : public support::NonCopyableBase,
                     public CancellableBase<Future> {
 public:
  explicit Vector(std::vector<Future> vec)
      : cached_size_(vec.size()),
        futures_(std::move(vec)) {
  }

  // Movable
  Vector(Vector&& that) noexcept
      : cached_size_(that.Size()),
        futures_(std::move(that.futures_)) {
  }
  Vector& operator=(Vector&&) = delete;

  std::vector<Future>& Peek() {
    return futures_;
  }

  size_t Size() const {
    return cached_size_;
  }

 private:
  const size_t cached_size_;
  std::vector<Future> futures_;
};

///////////////////////////////////////////////////////////////////////////////////////

template <Thunk Future, typename Block>
class ProducerForVector final : public support::PinnedBase {
  using InputType = typename Future::ValueType;

 public:
  explicit ProducerForVector(Future future)
      : eval_(std::move(future).Force(*this)) {
  }

  void Start(Block* block, size_t index) {
    block_ = block;
    index_ = index;
    eval_.Start();
  }

  // Completable
  void Consume(Output<InputType> o) noexcept {
    block_->Consume(std::move(o), index_);
  }

  // CancelSource
  void Cancel(Context) noexcept {
    block_->Cancel();
  }

  cancel::Token CancelToken() {
    return block_->CancelToken();
  }

 private:
  EvaluationType<ProducerForVector, Future> eval_;
  Block* block_{nullptr};
  size_t index_{0};
};

///////////////////////////////////////////////////////////////////////////////////////

template <typename T>
class PinnedStorage final : public support::PinnedBase {
 private:
  union PinnedSlot {
    PinnedSlot()
        : dummy_{} {
    }

    ~PinnedSlot(){
        // You must manually delete obj_!!!
    };

    Unit dummy_;
    T obj_;
  };

 public:
  template <typename U>
  requires std::is_constructible_v<T, U>
  explicit PinnedStorage(std::vector<U> vec)
      : impl_(vec.size()) {
    const size_t size = vec.size();

    for (size_t i = 0; i < size; i++) {
      new (&(impl_[i].obj_)) T(std::move(vec[i]));
    }
  }

  T& operator[](size_t ind) {
    return impl_[ind].obj_;
  }

  size_t Size() const {
    return impl_.size();
  }

  ~PinnedStorage() {
    const size_t size = impl_.size();

    for (size_t i = 0; i < size; i++) {
      std::destroy_at(&(impl_[i].obj_));
    }
  }

 private:
  std::vector<PinnedSlot> impl_;
};

///////////////////////////////////////////////////////////////////////////////////////

template <typename T, Thunk Future>
class TaggedVector final : public support::PinnedBase {
 public:
  explicit TaggedVector(Vector<Future> vec)
      : tagged_producers_(std::move(vec.Peek())) {
  }

  void BootUpFutures(T* block) {
    // Last StartAt may delete this so that .size() would be invalid to call
    const size_t size = tagged_producers_.Size();

    for (size_t i = 0; i < size; i++) {
      tagged_producers_[i].Start(block, i);
    }
  }

  size_t Size() const {
    return tagged_producers_.Size();
  }

 private:
  PinnedStorage<ProducerForVector<Future, T>> tagged_producers_;
};

}  // namespace weave::futures::thunks::detail