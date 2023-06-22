#pragma once

#include <weave/futures/thunks/combine/par/join.hpp>

#include <weave/futures/thunks/combine/par/all/tuple.hpp>
#include <weave/futures/thunks/combine/par/all/vector.hpp>

namespace weave::futures {

// Allocates

//////////////////////////////////////////////////////////////////////////////

template <SomeFuture InputFuture>
Future<std::vector<traits::ValueOf<InputFuture>>> auto All(
    std::vector<InputFuture> vec) {
  WHEELS_VERIFY(!vec.empty(), "Sending empty vector!");

  auto* block =
      new thunks::AllControlBlock<true, thunks::detail::Vector, InputFuture>(
          vec.size(), std::move(vec));

  return futures::thunks::Join(block);
}

template <SomeFuture First, typename... Futures>
Future<std::tuple<traits::ValueOf<First>, traits::ValueOf<Futures>...>> auto
All(First f1, Futures... fs) {
  static_assert((SomeFuture<Futures> && ...));
  auto* block = new thunks::AllControlBlock<true, thunks::detail::Tuple, First,
                                            Futures...>(
      1 + sizeof...(Futures), std::move(f1), std::move(fs)...);

  return thunks::Join(block);
}

template <SomeFuture First>
Future<traits::ValueOf<First>> auto All(First f1) {
  return std::move(f1);
}

// Actually vector version does allocate memory for outputs since it collects
// them in a vector But no_alloc versions do not allocate memory for input
// futures themselves

//////////////////////////////////////////////////////////////////////////////

namespace no_alloc {

template <traits::Cancellable InputFuture>
Future<std::vector<traits::ValueOf<InputFuture>>> auto All(
    std::vector<InputFuture> vec) {
  WHEELS_VERIFY(!vec.empty(), "Sending empty vector!");

  auto block =
      thunks::AllControlBlock<false, thunks::detail::Vector, InputFuture>(
          vec.size(), std::move(vec));

  return futures::thunks::JoinOnStack(std::move(block));
}

template <traits::Cancellable First, traits::Cancellable... Futures>
Future<std::tuple<traits::ValueOf<First>, traits::ValueOf<Futures>...>> auto
All(First f1, Futures... fs) {
  thunks::AllControlBlock<false, thunks::detail::Tuple, First, Futures...>
      block(1 + sizeof...(Futures), std::move(f1), std::move(fs)...);

  return thunks::JoinOnStack(std::move(block));
}

template <traits::Cancellable First>
Future<traits::ValueOf<First>> auto All(First f1) {
  return std::move(f1);
}

}  // namespace no_alloc

//////////////////////////////////////////////////////////////////////////////

// Backwards compatibility
template <SomeFuture LeftFuture, SomeFuture RightFuture>
Future<
    std::tuple<traits::ValueOf<LeftFuture>, traits::ValueOf<RightFuture>>> auto
Both(LeftFuture f1, RightFuture f2) {
  return All(std::move(f1), std::move(f2));
}

}  // namespace weave::futures
