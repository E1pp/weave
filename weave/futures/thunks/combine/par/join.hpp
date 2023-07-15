#pragma once

#include <weave/futures/old_types/future.hpp>

#include <weave/futures/thunks/detail/cancel_base.hpp>

namespace weave::futures::thunks {

template <typename Block>
struct [[nodiscard]] Join : public detail::JustCancellableBase<Block> {
 public:
  using ValueType = typename Block::ValueType;

  explicit Join(Block* block)
      : block_(block) {
  }

  // Non-copyable
  Join(const Join&) = delete;
  Join& operator=(const Join&) = delete;

  // Movable
  Join(Join&& that)
      : block_(that.block_){};
  Join& operator=(Join&&) = default;

  void Start(IConsumer<ValueType>* consumer) {
    block_->Start(consumer);
  }

 private:
  Block* block_;
};

////////////////////////////////////////////////////////////////////////

template <typename Block>
struct [[nodiscard]] JoinOnStack {
 public:
  using ValueType = typename Block::ValueType;

  explicit JoinOnStack(Block&& block)
      : block_(std::move(block)) {
  }

  // Non-copyable
  JoinOnStack(const JoinOnStack&) = delete;
  JoinOnStack& operator=(const JoinOnStack&) = delete;

  // Movable
  JoinOnStack(JoinOnStack&& that)
      : block_(std::move(that.block_)){};
  JoinOnStack& operator=(JoinOnStack&&) = default;

  void Start(IConsumer<ValueType>* consumer) {
    block_.Start(consumer);
  }

  void Cancellable() {
    // No-Op
  }

 private:
  Block block_;
};

}  // namespace weave::futures::thunks