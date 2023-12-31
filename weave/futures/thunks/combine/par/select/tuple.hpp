#pragma once

#include <weave/futures/thunks/combine/par/select/decl.hpp>

#include <weave/futures/thunks/combine/par/detail/sync_strategies/join_all.hpp>
#include <weave/futures/thunks/combine/par/detail/storage_types/tuple.hpp>
#include <weave/futures/thunks/combine/par/detail/join_block.hpp>

#include <weave/futures/traits/value_of.hpp>

#include <weave/result/make/err.hpp>
#include <weave/result/make/ok.hpp>

#include <optional>
#include <variant>

namespace weave::futures::thunks {

template <SomeFuture... Futures>
using SelectedValue = std::variant<Result<traits::ValueOf<Futures>>...>;

///////////////////////////////////////////////////////////////////////

// Default is OnHeap == true
template <bool OnHeap, typename Cons, SomeFuture... Futures>
class SelectControlBlock<OnHeap, Cons, detail::TaggedTuple, Futures...> final
    : public detail::JoinBlock<
          true, SelectControlBlock<true, Cons, detail::TaggedTuple, Futures...>,
          detail::JoinAll<true>, SelectedValue<Futures...>, Cons,
          detail::TaggedTuple, Futures...> {
 public:
  using ValueType = SelectedValue<Futures...>;
  using Base = detail::JoinBlock<
      true, SelectControlBlock<true, Cons, detail::TaggedTuple, Futures...>,
      detail::JoinAll<true>, ValueType, Cons, detail::TaggedTuple, Futures...>;

  template <typename InterStorage>
  requires std::is_constructible_v<Base, Cons&, InterStorage>
  SelectControlBlock(size_t, Cons& cons, InterStorage storage)
      : Base(cons, std::move(storage)) {
  }

  ~SelectControlBlock() override = default;

  template <size_t Index>
  void Consume(
      Output<detail::NthType<Index, traits::ValueOf<Futures>...>> out) {
    auto result = std::move(out.result);

    if (Base::MarkFulfilled()) {
      // We got the first result (either error or an actual value)
      ProduceResult<Index>(std::move(result));
    }

    Base::ProducerDone();
    Base::ReleaseRef();
  }

  void Cancel() {
    if (bool should_complete = Base::ProducerDone()) {
      // Entire tree got cancelled
      Base::CancelConsumer();
    }

    Base::ReleaseRef();
  }

 private:
  template <size_t Index>
  void ProduceResult(
      Result<detail::NthType<Index, traits::ValueOf<Futures>...>> res) {
    ValueType variant{std::in_place_index<Index>, std::move(res)};
    Base::CompleteConsumer(result::Ok(std::move(variant)));
  }
};

///////////////////////////////////////////////////////////////////////

// OnHeap == false
template <typename Cons, SomeFuture... Futures>
class SelectControlBlock<false, Cons, detail::TaggedTuple, Futures...> final
    : public detail::JoinBlock<
          false,
          SelectControlBlock<false, Cons, detail::TaggedTuple, Futures...>,
          detail::JoinAll<false>, SelectedValue<Futures...>, Cons,
          detail::TaggedTuple, Futures...> {
 public:
  using ValueType = SelectedValue<Futures...>;
  using Base = detail::JoinBlock<
      false, SelectControlBlock<false, Cons, detail::TaggedTuple, Futures...>,
      detail::JoinAll<false>, ValueType, Cons, detail::TaggedTuple, Futures...>;

  template <typename InterStorage>
  requires std::is_constructible_v<Base, Cons&, InterStorage>
  SelectControlBlock(size_t, Cons& cons, InterStorage storage)
      : Base(cons, std::move(storage)) {
  }

  ~SelectControlBlock() override = default;

  template <size_t Index>
  void Consume(
      Output<detail::NthType<Index, traits::ValueOf<Futures>...>> out) {
    auto result = std::move(out.result);

    if (Base::MarkFulfilled()) {
      // We got the first result (either error or an actual value)
      EmplaceResult<Index>(std::move(result));
    }

    if (bool is_the_last_one = Base::ProducerDone()) {
      WHEELS_VERIFY(res_, "Unfulfilled result!");

      Base::CompleteConsumer(std::move(*res_));
    }
  }

  void Cancel() {
    if (bool is_the_last_one = Base::ProducerDone()) {
      if (res_) {
        // We are cancelled by some other future fulfilling result
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
    ValueType variant{std::in_place_index<Index>, std::move(res)};
    res_.emplace(result::Ok(std::move(variant)));

    Base::Forward(cancel::Signal::Cancel());
  }

 private:
  std::optional<Result<ValueType>> res_;
};

}  // namespace weave::futures::thunks