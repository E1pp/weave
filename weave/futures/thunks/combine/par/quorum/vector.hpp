#pragma once

#include <weave/futures/thunks/combine/par/quorum/decl.hpp>

#include <weave/futures/thunks/combine/par/detail/sync_strategies/dummy.hpp>
#include <weave/futures/thunks/combine/par/detail/storage_types/vector.hpp>
#include <weave/futures/thunks/combine/par/detail/join_block.hpp>

#include <weave/futures/traits/value_of.hpp>

#include <weave/result/make/err.hpp>
#include <weave/result/make/ok.hpp>

#include <weave/threads/blocking/spinlock.hpp>
#include <weave/threads/blocking/stdlike/mutex.hpp>

#include <optional>

namespace weave::futures::thunks {

///////////////////////////////////////////////////////////////////////

// Default is OnHeap == true
template <bool OnHeap, typename Cons, SomeFuture Future>
class QuorumControlBlock<OnHeap, Cons, detail::TaggedVector, Future> final
    : public detail::JoinBlock<
          true, QuorumControlBlock<true, Cons, detail::TaggedVector, Future>,
          detail::Dummy, std::vector<traits::ValueOf<Future>>, Cons,
          detail::TaggedVector, Future> {
 public:
  using InputType = traits::ValueOf<Future>;
  using ValueType = std::vector<InputType>;
  using Base = detail::JoinBlock<
      true, QuorumControlBlock<true, Cons, detail::TaggedVector, Future>,
      detail::Dummy, ValueType, Cons, detail::TaggedVector, Future>;
  using Storage = detail::TaggedVector<
      QuorumControlBlock<true, Cons, detail::TaggedVector, Future>, Future>;
  using Guard =
      threads::blocking::stdlike::UniqueLock<threads::blocking::SpinLock>;

  template <typename InterStorage>
  requires std::is_constructible_v<Storage, InterStorage>
  explicit QuorumControlBlock(size_t threshold, Cons& cons,
                              InterStorage storage)
      : Base(cons, std::move(storage)),
        threshold_(threshold),
        capacity_(storage.Size()),
        active_producers_(capacity_) {
    storage_.reserve(threshold_);
  }

  ~QuorumControlBlock() override = default;

  // NB : reverse order of destruction spinlock.Unlock -> ReleaseRef
  void Consume(Output<InputType> out, size_t) {
    auto result = std::move(out.result);

    wheels::Defer cleanup([&] {
      Base::ReleaseRef();
    });

    Guard guard(spinlock_);

    if (result) {
      ProcessResult(std::move(result));
    } else {
      ProcessError(std::move(result));
    }

    ProducerDone();
  }

  // NB : reverse order of destruction spinlock.Unlock -> ReleaseRef
  void Cancel() {
    wheels::Defer cleanup([&] {
      Base::ReleaseRef();
    });

    Guard guard(spinlock_);

    // Cancel if you are the last one and Niether error nor result were commited
    if (ProducerDone() && ShouldProduce()) {
      // Unlock mutex to prevent heap use after free (scope)
      guard.Unlock();
      Base::CancelConsumer();
    }
  }

 private:
  void ProcessResult(Result<InputType> res) {
    if (!ShouldProduce()) {
      return;
    }

    storage_.push_back(std::move(*res));
    num_done_++;

    if (WasReached()) {
      Result<ValueType> matched_type{std::move(storage_)};
      Base::CompleteConsumer(std::move(matched_type));
    }
  }

  void ProcessError(Result<InputType> err) {
    if (!ShouldProduce()) {
      return;
    }

    num_errors_++;

    if (ImpossibleToReach()) {
      Result<ValueType> matched_type{result::Err(std::move(err.error()))};
      Base::CompleteConsumer(std::move(matched_type));
    }
  }

  // Helper functions
  bool WasReached() {
    return num_done_ == threshold_;
  }

  bool ImpossibleToReach() {
    return threshold_ + num_errors_ > capacity_;
  }

  bool ShouldProduce() {
    return !WasReached() && !ImpossibleToReach();
  }

  bool ProducerDone() {
    return active_producers_-- == 1;
  }

 private:
  const size_t threshold_;
  const size_t capacity_;
  size_t num_errors_{0};
  size_t num_done_{0};
  size_t active_producers_;

  std::vector<InputType> storage_{};

  // sync
  threads::blocking::SpinLock spinlock_;
};

///////////////////////////////////////////////////////////////////////

template <typename Cons, SomeFuture Future>
class QuorumControlBlock<false, Cons, detail::TaggedVector, Future> final
    : public detail::JoinBlock<
          false, QuorumControlBlock<false, Cons, detail::TaggedVector, Future>,
          detail::Dummy, std::vector<traits::ValueOf<Future>>, Cons,
          detail::TaggedVector, Future> {
 public:
  using InputType = traits::ValueOf<Future>;
  using ValueType = std::vector<InputType>;
  using Base = detail::JoinBlock<
      false, QuorumControlBlock<false, Cons, detail::TaggedVector, Future>,
      detail::Dummy, ValueType, Cons, detail::TaggedVector, Future>;
  using Storage = detail::TaggedVector<
      QuorumControlBlock<false, Cons, detail::TaggedVector, Future>, Future>;
  using Guard =
      threads::blocking::stdlike::UniqueLock<threads::blocking::SpinLock>;

  template <typename InterStorage>
  requires std::is_constructible_v<Storage, InterStorage>
  explicit QuorumControlBlock(size_t threshold, Cons& cons,
                              InterStorage storage)
      : Base(cons, std::move(storage)),
        threshold_(threshold),
        capacity_(storage.Size()),
        active_producers_(capacity_) {
    storage_.reserve(threshold_);
  }

  ~QuorumControlBlock() override = default;

  // NB : reverse order of destruction spinlock.Unlock -> ReleaseRef
  void Consume(Output<InputType> out, size_t) {
    auto result = std::move(out.result);

    Guard guard(spinlock_);

    if (result) {
      ProcessResult(std::move(result));
    } else {
      ProcessError(std::move(result));
    }

    if (ProducerDone()) {
      // Unlock mutex to prevent heap use after free (scope)
      guard.Unlock();
      Base::CompleteConsumer(std::move(*result_));
    }
  }

  // NB : reverse order of destruction spinlock.Unlock -> ReleaseRef
  void Cancel() {
    Guard guard(spinlock_);

    if (ProducerDone()) {
      // Unlock mutex to prevent heap use after free (scope)
      guard.Unlock();
      if (result_) {
        Base::CompleteConsumer(std::move(*result_));
      } else {
        Base::CancelConsumer();
      }
    }
  }

 private:
  void ProcessResult(Result<InputType> res) {
    if (!ShouldProduce()) {
      return;
    }

    storage_.push_back(std::move(*res));
    num_done_++;

    if (WasReached()) {
      Result<ValueType> matched_type{std::move(storage_)};

      result_.emplace(std::move(matched_type));

      Base::Forward(cancel::Signal::Cancel());
    }
  }

  void ProcessError(Result<InputType> err) {
    if (!ShouldProduce()) {
      return;
    }

    num_errors_++;

    if (ImpossibleToReach()) {
      Result<ValueType> matched_type{result::Err(std::move(err.error()))};

      result_.emplace(std::move(matched_type));

      Base::Forward(cancel::Signal::Cancel());
    }
  }

  // Helper functions
  bool WasReached() {
    return num_done_ == threshold_;
  }

  bool ImpossibleToReach() {
    return threshold_ + num_errors_ > capacity_;
  }

  bool ShouldProduce() {
    return !WasReached() && !ImpossibleToReach();
  }

  bool ProducerDone() {
    return active_producers_-- == 1;
  }

 private:
  const size_t threshold_;
  const size_t capacity_;
  size_t num_errors_{0};
  size_t num_done_{0};
  size_t active_producers_;

  std::optional<Result<ValueType>> result_;

  std::vector<InputType> storage_{};

  // sync
  threads::blocking::SpinLock spinlock_;
};

}  // namespace weave::futures::thunks