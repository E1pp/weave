#pragma once

#include <weave/futures/thunks/combine/par/join.hpp>

#include <weave/futures/thunks/combine/par/quorum/tuple.hpp>
#include <weave/futures/thunks/combine/par/quorum/vector.hpp>

#include <wheels/core/assert.hpp>

namespace weave::futures {

// Allocates

//////////////////////////////////////////////////////////////////////////////

template <SomeFuture InputFuture>
Future<std::vector<traits::ValueOf<InputFuture>>> auto Quorum(
    size_t threshold, std::vector<InputFuture> vec) {
  const size_t size = vec.size();
  WHEELS_VERIFY(size != 0, "Sending empty vector!");

  WHEELS_VERIFY(threshold > 0, "Threshold should be greater than zero!");
  WHEELS_VERIFY(threshold <= size, "Treshold is too big!");

  using Storage = thunks::detail::Vector<InputFuture>;
  using ValueType = std::vector<traits::ValueOf<InputFuture>>;

  using QuorumFuture =
      thunks::Join<true, /*OnHeap=*/true, ValueType, thunks::QuorumControlBlock,
                   Storage, thunks::detail::TaggedVector, InputFuture>;

  return QuorumFuture(threshold, std::move(vec));
}

template <SomeFuture First, typename... Suffix>
Future<
    thunks::QuorumType<traits::ValueOf<First>, traits::ValueOf<Suffix>...>> auto
Quorum(size_t threshold, First f1, Suffix... fs) {
  static_assert(
      (std::is_same_v<traits::ValueOf<First>, traits::ValueOf<Suffix>> && ...));

  const size_t size = 1 + sizeof...(Suffix);

  WHEELS_VERIFY(threshold > 0, "Threshold should be greater than zero!");
  WHEELS_VERIFY(threshold <= size, "Treshold is too big!");

  using Storage = thunks::detail::Tuple<First, Suffix...>;
  using ValueType =
      thunks::QuorumType<traits::ValueOf<First>, traits::ValueOf<Suffix>...>;

  using QuorumFuture =
      thunks::Join<true, /*OnHeap=*/true, ValueType, thunks::QuorumControlBlock,
                   Storage, thunks::detail::TaggedTuple, First, Suffix...>;

  return QuorumFuture(threshold, std::move(f1), std::move(fs)...);
}

template <SomeFuture First>
Future<traits::ValueOf<First>> auto Quorum(size_t threshold, First f1) {
  WHEELS_VERIFY(threshold > 0, "Threshold should be greater than zero!");
  WHEELS_VERIFY(threshold <= 1, "Treshold is too big!");

  return std::move(f1);
}

// Actually vector version does allocate memory for outputs since it collects
// them in a vector. But no_alloc versions do not allocate memory for input
// futures themselves

//////////////////////////////////////////////////////////////////////////////

namespace no_alloc {

template <traits::Cancellable InputFuture>
Future<std::vector<traits::ValueOf<InputFuture>>> auto Quorum(
    size_t threshold, std::vector<InputFuture> vec) {
  const size_t size = vec.size();
  WHEELS_VERIFY(size != 0, "Sending empty vector!");

  WHEELS_VERIFY(threshold > 0, "Threshold should be greater than zero!");
  WHEELS_VERIFY(threshold <= size, "Treshold is too big!");

  using Storage = thunks::detail::Vector<InputFuture>;
  using ValueType = std::vector<traits::ValueOf<InputFuture>>;

  using QuorumFuture = thunks::Join<true, /*OnHeap=*/false, ValueType,
                                    thunks::QuorumControlBlock, Storage,
                                    thunks::detail::TaggedVector, InputFuture>;

  return QuorumFuture(threshold, std::move(vec));
}

template <traits::Cancellable First, traits::Cancellable... Suffix>
Future<
    thunks::QuorumType<traits::ValueOf<First>, traits::ValueOf<Suffix>...>> auto
Quorum(size_t threshold, First f1, Suffix... fs) {
  static_assert(
      (std::is_same_v<traits::ValueOf<First>, traits::ValueOf<Suffix>> && ...));

  const size_t size = 1 + sizeof...(Suffix);

  WHEELS_VERIFY(threshold > 0, "Threshold should be greater than zero!");
  WHEELS_VERIFY(threshold <= size, "Treshold is too big!");

  using Storage = thunks::detail::Tuple<First, Suffix...>;
  using ValueType =
      thunks::QuorumType<traits::ValueOf<First>, traits::ValueOf<Suffix>...>;

  using QuorumFuture =
      thunks::Join<true, /*OnHeap=*/false, ValueType,
                   thunks::QuorumControlBlock, Storage,
                   thunks::detail::TaggedTuple, First, Suffix...>;

  return QuorumFuture(threshold, std::move(f1), std::move(fs)...);
}

template <SomeFuture First>
Future<traits::ValueOf<First>> auto Quorum(size_t threshold, First f1) {
  WHEELS_VERIFY(threshold > 0, "Threshold should be greater than zero!");
  WHEELS_VERIFY(threshold <= 1, "Treshold is too big!");

  return std::move(f1);
}

}  // namespace no_alloc

}  // namespace weave::futures