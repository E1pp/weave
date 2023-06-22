#pragma once

#include <weave/futures/thunks/combine/par/all/decl.hpp>

#include <weave/futures/thunks/combine/par/detail/sync_strategies/join_all.hpp>
#include <weave/futures/thunks/combine/par/detail/storage_types/tuple.hpp>
#include <weave/futures/thunks/combine/par/detail/join_block.hpp>

#include <weave/futures/traits/value_of.hpp>

#include <weave/result/make/err.hpp>
#include <weave/result/make/ok.hpp>

#include <weave/futures/thunks/detail/cancel_base.hpp>

#include <optional>

namespace weave::futures::thunks {

template <bool OnHeap, SomeFuture... Futures>
struct AllControlBlock<OnHeap, detail::Tuple, Futures...>
    : public detail::JoinBlock<
          true, std::tuple<traits::ValueOf<Futures>...>,
          AllControlBlock<OnHeap, detail::Tuple, Futures...>,
          detail::JoinAllOnHeap, detail::Tuple, Futures...>,
      public detail::VariadicCancellableBase<Futures...> {
 public:
  using ValueType = std::tuple<traits::ValueOf<Futures>...>;
  using Base =
      detail::JoinBlock<true, std::tuple<traits::ValueOf<Futures>...>,
                        AllControlBlock<OnHeap, detail::Tuple, Futures...>,
                        detail::JoinAllOnHeap, detail::Tuple, Futures...>;
  using Base::Base;

  ~AllControlBlock() override = default;

  void Create() {
    // No-Op
  }

  template <size_t Index>
  void Consume(
      Output<detail::NthType<Index, traits::ValueOf<Futures>...>> out) {
    auto result = std::move(out.result);

    // CompleteConsumer may throw
    wheels::Defer cleanup([&] {
      Base::ReleaseRef();
    });

    if (result) {
      std::get<Index>(storage_).emplace(std::move(*result));

    } else if (Base::MarkFulfilled()) {
      // we got the first error
      Result<ValueType> matched_type = result::Err(std::move(result.error()));
      Base::CompleteConsumer(std::move(matched_type));
    }

    if (bool should_complete = Base::ProducerDone()) {
      ValueType tuple;
      std::apply(
          [&tuple](auto... elem) {
            tuple = std::make_tuple(std::move(*elem)...);
          },
          std::move(storage_));

      Base::CompleteConsumer(result::Ok(std::move(tuple)));
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

 private:
  std::tuple<std::optional<traits::ValueOf<Futures>>...> storage_;
};

//////////////////////////////////////////////////////////////////////////

template <SomeFuture... Futures>
struct AllControlBlock<false, detail::Tuple, Futures...>
    : public detail::JoinBlock<
          false, std::tuple<traits::ValueOf<Futures>...>,
          AllControlBlock<false, detail::Tuple, Futures...>,
          detail::JoinAllOnStack, detail::Tuple, Futures...>,
      public detail::VariadicCancellableBase<Futures...> {
 public:
  using ValueType = std::tuple<traits::ValueOf<Futures>...>;
  using Base =
      detail::JoinBlock<false, std::tuple<traits::ValueOf<Futures>...>,
                        AllControlBlock<false, detail::Tuple, Futures...>,
                        detail::JoinAllOnStack, detail::Tuple, Futures...>;
  using Base::Base;

  // Non-copyable
  AllControlBlock(const AllControlBlock&) = delete;
  AllControlBlock& operator=(const AllControlBlock&) = delete;

  // Movable
  AllControlBlock(AllControlBlock&&) = default;
  AllControlBlock& operator=(AllControlBlock&&) = default;

  ~AllControlBlock() override = default;

  void Create() {
    // No-Op
  }

  template <size_t Index>
  void Consume(
      Output<detail::NthType<Index, traits::ValueOf<Futures>...>> out) {
    auto result = std::move(out.result);

    if (result) {
      std::get<Index>(storage_).emplace(std::move(*result));
    } else if (Base::MarkFulfilled()) {
      // we got the first error
      Result<ValueType> matched_type = result::Err(std::move(result.error()));
      EmplaceError(std::move(matched_type));
    }

    if (bool is_the_last_one = Base::ProducerDone()) {
      if (err_) {
        Base::CompleteConsumer(std::move(*err_));
      } else {
        ValueType tuple;
        std::apply(
            [&tuple](auto... elem) {
              tuple = std::make_tuple(std::move(*elem)...);
            },
            std::move(storage_));

        Base::CompleteConsumer(result::Ok(std::move(tuple)));
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
  std::tuple<std::optional<traits::ValueOf<Futures>>...> storage_;
  std::optional<Result<ValueType>> err_;
};

}  // namespace weave::futures::thunks