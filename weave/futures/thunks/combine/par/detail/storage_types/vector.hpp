#pragma once

#include <weave/futures/old_types/future.hpp>

#include <optional>

namespace weave::futures::thunks::detail {

///////////////////////////////////////////////////////////////////////////////////////

template <SomeFuture Future, typename Block>
class ProducerForVector final : public IConsumer<typename Future::ValueType> {
  using InputType = typename Future::ValueType;

 public:
  explicit ProducerForVector(Future future)
      : future_(std::move(future)) {
  }

  // Non-copyable
  ProducerForVector(const ProducerForVector&) = delete;
  ProducerForVector& operator=(const ProducerForVector&) = delete;

  // Movable
  ProducerForVector(ProducerForVector&& that)
      : future_(std::move(that.future_)),
        block_(that.block_) {
  }
  ProducerForVector& operator=(ProducerForVector&&) = default;

  void StartAt(Block* block, size_t index) {
    block_ = block;
    index_ = index;
    future_.Start(this);
  }

 private:
  void Consume(Output<InputType> out) noexcept override final {
    block_->Consume(std::move(out), index_);
  }

  void Cancel(Context) noexcept override final {
    block_->Cancel();
  }

  cancel::Token CancelToken() override final {
    return block_->CancelToken();
  }

 private:
  Future future_;
  Block* block_{nullptr};
  size_t index_{0};
};

///////////////////////////////////////////////////////////////////////////////////////

template <typename T, SomeFuture Future>
class Vector {
 public:
  explicit Vector(std::vector<Future> vec)
      : futures_{} {
    for (auto& future : vec) {
      ProducerForVector<Future, T> instance(std::move(future));
      futures_.push_back(std::move(instance));
    }
  }

  // Non-copyable
  Vector(const Vector&) = delete;
  Vector& operator=(const Vector&) = delete;

  // Movable
  Vector(Vector&& that)
      : futures_(std::move(that.futures_)) {
  }
  Vector& operator=(Vector&&) = default;

  void BootUpFutures(T* block) {
    // Last StartAt may delete this so that .size() would be invalid to call
    const size_t size = futures_.size();

    for (size_t i = 0; i < size; i++) {
      futures_[i].StartAt(block, i);
    }
  }

  size_t Size() const {
    size_t size = futures_.size();
    return size;
  }

 private:
  std::vector<ProducerForVector<Future, T>> futures_;
};

}  // namespace weave::futures::thunks::detail