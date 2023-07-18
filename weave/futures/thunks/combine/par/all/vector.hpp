#pragma once

#include <weave/futures/thunks/combine/par/all/decl.hpp>

#include <weave/futures/thunks/combine/par/detail/sync_strategies/join_all.hpp>
#include <weave/futures/thunks/combine/par/detail/storage_types/vector.hpp>
#include <weave/futures/thunks/combine/par/detail/join_block.hpp>

#include <weave/futures/traits/value_of.hpp>

#include <weave/result/make/err.hpp>
#include <weave/result/make/ok.hpp>

#include <optional>

namespace weave::futures::thunks {

template <bool OnHeap, typename Cons, SomeFuture Future>
class AllControlBlock<OnHeap, Cons, detail::TaggedVector, Future> final
    : public detail::JoinBlock<
          true, AllControlBlock<true, Cons, detail::TaggedVector, Future>,
          detail::JoinAll<true>, std::vector<traits::ValueOf<Future>>, Cons,
          detail::TaggedVector, Future> {
 public:
  using InputType = traits::ValueOf<Future>;
  using ValueType = std::vector<InputType>;
  using Base = detail::JoinBlock<
      true, AllControlBlock<true, Cons, detail::TaggedVector, Future>,
      detail::JoinAll<true>, ValueType, Cons, detail::TaggedVector, Future>;

  template <typename InterStorage>
  requires std::is_constructible_v<Base, Cons&, InterStorage> AllControlBlock(
      size_t, Cons& cons, InterStorage storage)
      : Base(cons, std::move(storage)),
        storage_(storage.Size()) {
  }

  ~AllControlBlock() override = default;

  void Consume(Output<InputType> out, size_t index) {
    auto result = std::move(out.result);

    wheels::Defer cleanup([&] {
      Base::ReleaseRef();
    });

    if (result) {
      storage_[index].emplace(std::move(*result));

    } else if (Base::MarkFulfilled()) {
      // we got the first error
      Result<ValueType> matched_type = result::Err(result.error());
      Base::CompleteConsumer(std::move(matched_type));
    }

    if (bool should_complete = Base::ProducerDone()) {
      ValueType vector{};

      for (size_t i = 0; i < storage_.size(); i++) {
        vector.push_back(std::move(*(storage_[i])));
      }

      Base::CompleteConsumer(result::Ok(std::move(vector)));
    }
  }

  void Cancel() {
    wheels::Defer cleanup([&] {
      Base::ReleaseRef();
    });

    if (bool should_complete = Base::ProducerDone()) {
      Base::CancelConsumer();
    }
  }

 private:
  std::vector<std::optional<InputType>> storage_;
};

//////////////////////////////////////////////////////////////////////////

template <typename Cons, SomeFuture Future>
class AllControlBlock<false, Cons, detail::TaggedVector, Future> final
    : public detail::JoinBlock<
          false, AllControlBlock<false, Cons, detail::TaggedVector, Future>,
          detail::JoinAll<false>, std::vector<traits::ValueOf<Future>>, Cons,
          detail::TaggedVector, Future> {
 public:
  using InputType = traits::ValueOf<Future>;
  using ValueType = std::vector<InputType>;
  using Base = detail::JoinBlock<
      false, AllControlBlock<false, Cons, detail::TaggedVector, Future>,
      detail::JoinAll<false>, ValueType, Cons, detail::TaggedVector, Future>;

  template <typename InterStorage>
  requires std::is_constructible_v<Base, Cons&, InterStorage> AllControlBlock(
      size_t, Cons& cons, InterStorage storage)
      : Base(cons, std::move(storage)),
        storage_(storage.Size()) {
  }

  ~AllControlBlock() override = default;

  void Consume(Output<InputType> out, size_t index) {
    auto result = std::move(out.result);

    if (result) {
      storage_[index].emplace(std::move(*result));
    } else if (Base::MarkFulfilled()) {
      // we got the first error
      Result<ValueType> matched_type = result::Err(result.error());
      EmplaceError(std::move(matched_type));
    }

    if (bool is_the_last_one = Base::ProducerDone()) {
      if (err_) {
        Base::CompleteConsumer(std::move(*err_));
      } else {
        ValueType vector{};
        for (size_t i = 0; i < storage_.size(); i++) {
          vector.push_back(std::move(*(storage_[i])));
        }

        Base::CompleteConsumer(result::Ok(std::move(vector)));
      }
    }
  }

  // Being cancelled implies cancel from above
  // or first error
  void Cancel() {
    if (bool is_the_last_one = Base::ProducerDone()) {
      if (err_) {
        Base::CompleteConsumer(std::move(*err_));
      } else {
        Base::CancelConsumer();
      }
    }
  }

 private:
  void EmplaceError(Result<ValueType> err) {
    // emplace error
    err_.emplace(result::Err(err.error()));

    // Cancel the rest
    Base::Forward(cancel::Signal::Cancel());
  }

 private:
  std::vector<std::optional<InputType>> storage_;
  std::optional<Result<ValueType>> err_;
};

}  // namespace weave::futures::thunks