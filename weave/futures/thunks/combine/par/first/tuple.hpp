#pragma once

#include <weave/futures/thunks/combine/par/first/decl.hpp>

#include <weave/futures/thunks/combine/par/detail/sync_strategies/join_all.hpp>
#include <weave/futures/thunks/combine/par/detail/storage_types/tuple.hpp>
#include <weave/futures/thunks/combine/par/detail/join_block.hpp>

#include <weave/futures/old_traits/value_of.hpp>

#include <weave/result/make/err.hpp>
#include <weave/result/make/ok.hpp>

#include <weave/futures/thunks/detail/cancel_base.hpp>

#include <optional>

namespace weave::futures::thunks {

template <typename... Ts>
struct FirstTypeImpl {};

template <typename T, typename... Ts>
requires(std::same_as<T, Ts>&&...) struct FirstTypeImpl<T, Ts...> {
  using Type = T;
};

template <typename... Ts>
using FirstType = typename FirstTypeImpl<Ts...>::Type;

///////////////////////////////////////////////////////////////////////

// Default is OnHeap == true
template <bool OnHeap, SomeFuture... Futures>
struct FirstControlBlock<OnHeap, detail::Tuple, Futures...>
    : public detail::JoinBlock<
          true, FirstType<traits::ValueOf<Futures>...>,
          FirstControlBlock<OnHeap, detail::Tuple, Futures...>,
          detail::JoinAllOnHeap, detail::Tuple, Futures...>,
      public detail::VariadicCancellableBase<Futures...> {
 public:
  using ValueType = FirstType<traits::ValueOf<Futures>...>;
  using Base =
      detail::JoinBlock<true, ValueType,
                        FirstControlBlock<OnHeap, detail::Tuple, Futures...>,
                        detail::JoinAllOnHeap, detail::Tuple, Futures...>;

  using Base::Base;

  void Create() {
    // No-Op
  }

  ~FirstControlBlock() override = default;

  template <size_t Index>
  void Consume(
      Output<detail::NthType<Index, traits::ValueOf<Futures>...>> out) {
    auto result = std::move(out.result);

    // CompleteConsumer may throw
    wheels::Defer cleanup([&] {
      Base::ReleaseRef();
    });

    if (result && Base::MarkFulfilled()) {
      // we are the first result
      Base::CompleteConsumer(std::move(result));
    }

    if (bool should_complete = Base::ProducerDone()) {
      Base::CompleteConsumer(std::move(result));
    }
  }

  void Cancel() {
    // consumer->Cancel() may throw
    wheels::Defer cleanup([&] {
      Base::ReleaseRef();
    });

    if (bool should_complete = Base::ProducerDone()) {
      Base::CancelConsumer();
    }
  }
};

///////////////////////////////////////////////////////////////////////

// OnStack spec which waits for the last future to cancel
template <SomeFuture... Futures>
struct FirstControlBlock<false, detail::Tuple, Futures...>
    : public detail::JoinBlock<
          false, FirstType<traits::ValueOf<Futures>...>,
          FirstControlBlock<false, detail::Tuple, Futures...>,
          detail::JoinAllOnStack, detail::Tuple, Futures...>,
      public detail::VariadicCancellableBase<Futures...> {
 public:
  using ValueType = FirstType<traits::ValueOf<Futures>...>;
  using Base =
      detail::JoinBlock<false, ValueType,
                        FirstControlBlock<false, detail::Tuple, Futures...>,
                        detail::JoinAllOnStack, detail::Tuple, Futures...>;

  using Base::Base;

  // Non-copyable
  FirstControlBlock(const FirstControlBlock&) = delete;
  FirstControlBlock& operator=(const FirstControlBlock&) = delete;

  // Movable
  FirstControlBlock(FirstControlBlock&&) = default;
  FirstControlBlock& operator=(FirstControlBlock&&) = default;

  ~FirstControlBlock() override = default;

  void Create() {
    // No-Op
  }

  template <size_t Index>
  void Consume(
      Output<detail::NthType<Index, traits::ValueOf<Futures>...>> out) {
    auto result = std::move(out.result);

    if (result && Base::MarkFulfilled()) {
      // we are the first result
      EmplaceResult(std::move(result));
    }

    if (bool is_the_last = Base::ProducerDone()) {
      // we are the last one
      if (res_) {
        Base::CompleteConsumer(std::move(*res_));
      } else {
        Base::CompleteConsumer(std::move(result));
      }
    }
  }

  void Cancel() {
    if (bool is_the_last = Base::ProducerDone()) {
      if (res_) {
        Base::CompleteConsumer(std::move(*res_));
      } else {
        Base::CancelConsumer();
      }
    }
  }

 private:
  void EmplaceResult(Result<ValueType> res) {
    // Emplace the result
    res_.emplace(std::move(res));

    // Cancel the rest
    Base::Forward(cancel::Signal::Cancel());
  }

 private:
  std::optional<Result<ValueType>> res_;
};

}  // namespace weave::futures::thunks