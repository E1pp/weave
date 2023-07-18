#pragma once

#include <weave/futures/model/evaluation.hpp>

#include <weave/futures/thunks/detail/cancel_base.hpp>

#include <weave/support/constructor_bases.hpp>

#include <tuple>

namespace weave::futures::thunks::detail {

template <size_t N, typename First, typename... Pack>
struct NthTypeImpl {
  using Type = typename NthTypeImpl<N - 1, Pack...>::Type;
};

template <typename First, typename... Pack>
struct NthTypeImpl<0, First, Pack...> {
  using Type = First;
};

template <size_t N, typename... Pack>
using NthType = typename NthTypeImpl<N, Pack...>::Type;

///////////////////////////////////////////////////////////////////////////////////////

template <Thunk... Future>
class Tuple final : public support::NonCopyableBase, public VariadicCancellableBase<Future...> {
 public:
  explicit Tuple(Future... fs)
      : futures_(std::make_tuple(std::move(fs)...)) {
  }

  // Movable
  Tuple(Tuple&& that) noexcept
      : futures_(std::move(that.futures_)) {
  }
  Tuple& operator=(Tuple&&) = delete;

  std::tuple<Future...>& Peek() {
    return futures_;
  }

  size_t Size() const {
    return sizeof...(Future);
  }

 private:
  std::tuple<Future...> futures_;
};

///////////////////////////////////////////////////////////////////////////////////////

template <Thunk Future, size_t Index, typename ControlBlock>
class ProducerForTuple {
  using InputType = typename Future::ValueType;

 public:
  explicit ProducerForTuple(Future future)
      : eval_(std::move(future).Force(*this)) {
  }

  void Start(ControlBlock* block) {
    block_ = block;
    eval_.Start();
  }

  // Completable
  void Consume(Output<InputType> out) noexcept {
    block_->template Consume<Index>(std::move(out));
  }

  // CancelSource
  void Cancel(Context) noexcept {
    block_->Cancel();
  }

  cancel::Token CancelToken() {
    return block_->CancelToken();
  }

 private:
  EvaluationType<ProducerForTuple, Future> eval_;
  ControlBlock* block_;
};

template <typename Block, typename Seq, Thunk... Future>
struct TaggedBase {};

template <typename Block, size_t... Index, Thunk... Future>
struct TaggedBase<Block, std::index_sequence<Index...>, Future...>
    : public ProducerForTuple<Future, Index, Block>... {
 public:
  explicit TaggedBase(Tuple<Future...> tupl)
      : ProducerForTuple<Future, Index, Block>(
            std::move(std::get<Index>(tupl.Peek())))... {
  }
};

///////////////////////////////////////////////////////////////////////////////////////

template <typename ControlBlock, Thunk... Futures>
class TaggedTuple final
    : public TaggedBase<ControlBlock, std::make_index_sequence<sizeof...(Futures)>, Futures...>,
      public support::PinnedBase {
 public:
  using Base =
      TaggedBase<ControlBlock, std::make_index_sequence<sizeof...(Futures)>,
                 Futures...>;

  using Base::Base;

  void BootUpFutures(ControlBlock* block) {
    [&]<size_t... Index>(std::index_sequence<Index...>) {
      (static_cast<ProducerForTuple<Futures, Index, ControlBlock>*>(this)
           ->Start(block),
       ...);
    }
    (std::index_sequence_for<Futures...>());
  }

  size_t Size() const {
    return sizeof...(Futures);
  }
};

}  // namespace weave::futures::thunks::detail