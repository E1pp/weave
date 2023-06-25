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
  WHEELS_VERIFY(!vec.empty(), "Sending empty vector!");

  WHEELS_VERIFY(threshold > 0, "Threshold should be greater than zero!");
  WHEELS_VERIFY(threshold <= vec.size(), "Treshold is too big!");

  auto* block =
      new thunks::QuorumControlBlock<true, thunks::detail::Vector, InputFuture>(
          threshold, vec.size(), std::move(vec));

  return futures::thunks::Join(block);
}

template <SomeFuture First, typename... Suffix>
Future<
    thunks::QuorumType<traits::ValueOf<First>, traits::ValueOf<Suffix>...>> auto
Quorum(size_t threshold, First f1, Suffix... fs) {
  static_assert(
      (std::is_same_v<traits::ValueOf<First>, traits::ValueOf<Suffix>> && ...));

  WHEELS_VERIFY(threshold > 0, "Threshold should be greater than zero!");
  WHEELS_VERIFY(threshold <= sizeof...(Suffix) + 1, "Treshold is too big!");

  auto* block = new thunks::QuorumControlBlock<true, thunks::detail::Tuple,
                                               First, Suffix...>(
      threshold, 1 + sizeof...(Suffix), std::move(f1), std::move(fs)...);

  return futures::thunks::Join(block);
}

template <SomeFuture First>
Future<traits::ValueOf<First>> auto Quorum(size_t threshold, First f1) {
  WHEELS_VERIFY(threshold > 0, "Threshold should be greater than zero!");
  WHEELS_VERIFY(threshold <= 1, "Treshold is too big!");

  return std::move(f1);
}

// Actually vector version does allocate memory for outputs since it collects
// them in a vector But no_alloc versions do not allocate memory for input
// futures themselves

//////////////////////////////////////////////////////////////////////////////

namespace no_alloc {

template <traits::Cancellable InputFuture>
Future<std::vector<traits::ValueOf<InputFuture>>> auto Quorum(
    size_t threshold, std::vector<InputFuture> vec) {
  WHEELS_VERIFY(!vec.empty(), "Sending empty vector!");

  WHEELS_VERIFY(threshold > 0, "Threshold should be greater than zero!");
  WHEELS_VERIFY(threshold <= vec.size(), "Treshold is too big!");

  auto block =
      thunks::QuorumControlBlock<false, thunks::detail::Vector, InputFuture>(
          threshold, vec.size(), std::move(vec));

  return futures::thunks::JoinOnStack(std::move(block));
}

template <traits::Cancellable First, traits::Cancellable... Suffix>
Future<
    thunks::QuorumType<traits::ValueOf<First>, traits::ValueOf<Suffix>...>> auto
Quorum(size_t threshold, First f1, Suffix... fs) {
  static_assert(
      (std::is_same_v<traits::ValueOf<First>, traits::ValueOf<Suffix>> && ...));

  WHEELS_VERIFY(threshold > 0, "Threshold should be greater than zero!");
  WHEELS_VERIFY(threshold <= sizeof...(Suffix) + 1, "Treshold is too big!");

  auto block = thunks::QuorumControlBlock<false, thunks::detail::Tuple, First,
                                          Suffix...>(
      threshold, 1 + sizeof...(Suffix), std::move(f1), std::move(fs)...);

  return futures::thunks::JoinOnStack(std::move(block));
}

template <SomeFuture First>
Future<traits::ValueOf<First>> auto Quorum(size_t threshold, First f1) {
  WHEELS_VERIFY(threshold > 0, "Threshold should be greater than zero!");
  WHEELS_VERIFY(threshold <= 1, "Treshold is too big!");

  return std::move(f1);
}

}  // namespace no_alloc

}  // namespace weave::futures