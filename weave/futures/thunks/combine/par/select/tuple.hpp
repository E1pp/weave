#pragma once

#include <weave/futures/thunks/combine/par/select/decl.hpp>

#include <weave/futures/thunks/combine/par/detail/sync_strategies/join_all.hpp>
#include <weave/futures/thunks/combine/par/detail/storage_types/tuple.hpp>
#include <weave/futures/thunks/combine/par/detail/join_block.hpp>

#include <weave/futures/traits/value_of.hpp>

#include <weave/result/make/err.hpp>
#include <weave/result/make/ok.hpp>

#include <weave/futures/thunks/detail/cancel_base.hpp>

#include <optional>
#include <variant>

namespace weave::futures::thunks {

template <SomeFuture... Futures>
using SelectedValue = std::variant<traits::ValueOf<Futures>...>;

///////////////////////////////////////////////////////////////////////

// Default is OnHeap == true
template <bool OnHeap, SomeFuture... Futures>
struct SelectControlBlock<OnHeap, detail::Tuple, Futures...>
    : public detail::JoinBlock<
          true, SelectedValue<Futures...>,
          SelectControlBlock<OnHeap, detail::Tuple, Futures...>,
          detail::JoinAllOnHeap, detail::Tuple, Futures...>,
      public detail::VariadicCancellableBase<Futures...> {
 public:
  using ValueType = SelectedValue<Futures...>;
  using Base =
      detail::JoinBlock<true, ValueType,
                        SelectControlBlock<OnHeap, detail::Tuple, Futures...>,
                        detail::JoinAllOnHeap, detail::Tuple, Futures...>;

  using Base::Base;

  void Create() {
    // No-Op
  }

  ~SelectControlBlock() override = default;

  template <size_t Index>
  void Consume(
      Output<detail::NthType<Index, traits::ValueOf<Futures>...>> out) {
    auto result = std::move(out.result);

    wheels::Defer cleanup([&] {
      Base::ReleaseRef();
    });

    if (result && Base::MarkFulfilled()) {
      // We got the first result
      ProduceResult<Index>(std::move(result));
    }

    if (bool should_complete = Base::ProducerDone()) {
      // We got the last error
      ProduceError<Index>(std::move(result));
    }
  }

  void Cancel() {
    wheels::Defer cleanup([&] {
      Base::ReleaseRef();
    });

    if (bool should_complete = Base::ProducerDone()) {
      // Entire tree got cancelled
      Base::CancelConsumer();
    }
  }

 private:
  template <size_t Index>
  void ProduceResult(
      Result<detail::NthType<Index, traits::ValueOf<Futures>...>> res) {
    ValueType variant{std::in_place_index<Index>, std::move(*res)};

    Base::CompleteConsumer(result::Ok(std::move(variant)));
  }

  template <size_t Index>
  void ProduceError(
      Result<detail::NthType<Index, traits::ValueOf<Futures>...>> err) {
    Result<ValueType> matched_type{result::Err(std::move(err.error()))};

    Base::CompleteConsumer(std::move(matched_type));
  }
};

///////////////////////////////////////////////////////////////////////

// OnStack spec which waits for the last future to cancel
template <SomeFuture... Futures>
struct SelectControlBlock<false, detail::Tuple, Futures...>
    : public detail::JoinBlock<
          false, SelectedValue<Futures...>,
          SelectControlBlock<false, detail::Tuple, Futures...>,
          detail::JoinAllOnStack, detail::Tuple, Futures...>,
      public detail::VariadicCancellableBase<Futures...> {
 public:
  using ValueType = SelectedValue<Futures...>;
  using Base =
      detail::JoinBlock<false, ValueType,
                        SelectControlBlock<false, detail::Tuple, Futures...>,
                        detail::JoinAllOnStack, detail::Tuple, Futures...>;

  using Base::Base;

  // Non-copyable
  SelectControlBlock(const SelectControlBlock&) = delete;
  SelectControlBlock& operator=(const SelectControlBlock&) = delete;

  // Movable
  SelectControlBlock(SelectControlBlock&&) = default;
  SelectControlBlock& operator=(SelectControlBlock&&) = default;

  void Create() {
    // No-Op
  }

  ~SelectControlBlock() override = default;

  template <size_t Index>
  void Consume(
      Output<detail::NthType<Index, traits::ValueOf<Futures>...>> out) {
    auto result = std::move(out.result);

    if (result && Base::MarkFulfilled()) {
      // We got the first result
      EmplaceResult<Index>(std::move(result));
    }

    if (bool is_the_last_one = Base::ProducerDone()) {
      if (!res_) {
        // Emplace the last error
        EmplaceError<Index>(std::move(result));
      }

      Base::CompleteConsumer(std::move(*res_));
    }
  }

  void Cancel() {
    if (bool is_the_last_one = Base::ProducerDone()) {
      if (res_) {
        // Someone got through
        Base::CompleteConsumer(std::move(*res_));
      } else {
        // Entire tree got cancelled
        Base::CancelConsumer();
      }
    }
  }

 private:
  template <size_t Index>
  void EmplaceResult(
      Result<detail::NthType<Index, traits::ValueOf<Futures>...>> res) {
    ValueType variant{std::in_place_index<Index>, std::move(*res)};

    res_.emplace(result::Ok(std::move(variant)));

    Base::Forward(cancel::Signal::Cancel());
  }

  template <size_t Index>
  void EmplaceError(
      Result<detail::NthType<Index, traits::ValueOf<Futures>...>> err) {
    Result<ValueType> matched_type{result::Err(std::move(err.error()))};

    res_.emplace(std::move(matched_type));
  }

 private:
  std::optional<Result<ValueType>> res_;
};

//   template <size_t Index>
//   void Consume(
//       Output<detail::NthType<Index, traits::ValueOf<Futures>...>> out) {
//     auto result = std::move(out.result);

//     if (result && Base::MarkFulfilled()) {
//       // we are the first result
//       EmplaceResult(std::move(result));
//     }

//     if (bool is_the_last = Base::ProducerDone()) {
//       // we are the last one
//       if (res_) {
//         Base::CompleteConsumer(std::move(*res_));
//       } else {
//         Base::CompleteConsumer(std::move(result));
//       }
//     }
//   }

//   void Cancel() {
//     if (bool is_the_last = Base::ProducerDone()) {
//       if (res_) {
//         Base::CompleteConsumer(std::move(*res_));
//       } else {
//         Base::CancelConsumer();
//       }
//     }
//   }

//  private:
//   void EmplaceResult(Result<ValueType> res) {
//     // Emplace the result
//     res_.emplace(std::move(res));

//     // Cancel the rest
//     Base::Forward(cancel::Signal::Cancel());
//   }

//  private:
//   std::optional<Result<ValueType>> res_;
// };

}  // namespace weave::futures::thunks