#pragma once

#include <weave/futures/old_types/future.hpp>

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

template <SomeFuture Future, size_t Index, typename Block>
class ProducerForTuple final : public IConsumer<typename Future::ValueType> {
  using InputType = typename Future::ValueType;

 public:
  explicit ProducerForTuple(Future future)
      : future_(std::move(future)) {
  }

  // Non-copyable
  ProducerForTuple(const ProducerForTuple&) = delete;
  ProducerForTuple& operator=(const ProducerForTuple&) = delete;

  // Movable
  ProducerForTuple(ProducerForTuple&& that)
      : future_(std::move(that.future_)),
        block_(that.block_) {
  }
  ProducerForTuple& operator=(ProducerForTuple&&) = default;

  void StartAt(Block* block) {
    block_ = block;
    future_.Start(this);
  }

 private:
  void Consume(Output<InputType> out) noexcept override final {
    block_->template Consume<Index>(std::move(out));
  }

  void Cancel(Context) noexcept override final {
    block_->Cancel();
  }

  cancel::Token CancelToken() override final {
    return block_->CancelToken();
  }

 private:
  Future future_;
  Block* block_;
};

///////////////////////////////////////////////////////////////////////////////////////

template <typename T, SomeFuture... Futures>
class Tuple {
 public:
  explicit Tuple(Futures... futures)
      : tagged_futures_(MakeTuple(std::move(futures)...)) {
  }

  template <SomeFuture... Ts>
  static auto MakeTuple(Ts... args) {
    return [&]<size_t... Is>(std::index_sequence<Is...>) {
      return std::make_tuple(ProducerForTuple<Ts, Is, T>(std::move(args))...);
    }
    (std::index_sequence_for<Ts...>());
  }

  // Non-copyable
  Tuple(const Tuple&) = delete;
  Tuple& operator=(const Tuple&) = delete;

  // Movable
  Tuple(Tuple&& that)
      : tagged_futures_(std::move(that.tagged_futures_)) {
  }
  Tuple& operator=(Tuple&&) = default;

  using TupleType = decltype(MakeTuple(std::declval<Futures>()...));

  void BootUpFutures(T* block) {
    std::apply(
        [&](auto&... tagged) {
          (tagged.StartAt(block), ...);
        },
        tagged_futures_);
  }

  size_t Size() const {
    return sizeof...(Futures);
  }

 private:
  TupleType tagged_futures_;
};

}  // namespace weave::futures::thunks::detail