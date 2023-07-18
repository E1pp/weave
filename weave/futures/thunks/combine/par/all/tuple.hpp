#pragma once

#include <weave/futures/thunks/combine/par/all/decl.hpp>

#include <weave/futures/thunks/combine/par/detail/sync_strategies/join_all.hpp>
#include <weave/futures/thunks/combine/par/detail/storage_types/tuple.hpp>
#include <weave/futures/thunks/combine/par/detail/join_block.hpp>

#include <weave/futures/traits/value_of.hpp>

#include <weave/result/make/err.hpp>
#include <weave/result/make/ok.hpp>

#include <optional>

namespace weave::futures::thunks {

// Default is OnHeap == true
template <bool OnHeap, typename Cons, SomeFuture... Futures>
class AllControlBlock<OnHeap, Cons, detail::TaggedTuple, Futures...> final
    : public detail::JoinBlock<
          true, AllControlBlock<true, Cons, detail::TaggedTuple, Futures...>,
          detail::JoinAll<true>, std::tuple<traits::ValueOf<Futures>...>, Cons,
          detail::TaggedTuple, Futures...> {
 public:
  using ValueType = std::tuple<traits::ValueOf<Futures>...>;
  using Base = detail::JoinBlock<
      true, AllControlBlock<true, Cons, detail::TaggedTuple, Futures...>,
      detail::JoinAll<true>, ValueType, Cons, detail::TaggedTuple, Futures...>;

  template <typename InterStorage>
  requires std::is_constructible_v<Base, Cons&, InterStorage> AllControlBlock(
      size_t, Cons& cons, InterStorage storage)
      : Base(cons, std::move(storage)) {
  }

  ~AllControlBlock() override = default;

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

template <typename Cons, SomeFuture... Futures>
class AllControlBlock<false, Cons, detail::TaggedTuple, Futures...> final
    : public detail::JoinBlock<
          false, AllControlBlock<false, Cons, detail::TaggedTuple, Futures...>,
          detail::JoinAll<false>, std::tuple<traits::ValueOf<Futures>...>, Cons,
          detail::TaggedTuple, Futures...> {
 public:
  using ValueType = std::tuple<traits::ValueOf<Futures>...>;
  using Base = detail::JoinBlock<
      false, AllControlBlock<false, Cons, detail::TaggedTuple, Futures...>,
      detail::JoinAll<false>, ValueType, Cons, detail::TaggedTuple, Futures...>;

  template <typename InterStorage>
  requires std::is_constructible_v<Base, Cons&, InterStorage> AllControlBlock(
      size_t, Cons& cons, InterStorage storage)
      : Base(cons, std::move(storage)) {
  }

  ~AllControlBlock() override = default;

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