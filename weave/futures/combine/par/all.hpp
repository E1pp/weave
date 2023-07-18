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

  using Storage = thunks::detail::Vector<InputFuture>;
  using ValueType = std::vector<traits::ValueOf<InputFuture>>;

  using AllFuture =
      thunks::Join<true, /*OnHeap=*/true, ValueType, thunks::AllControlBlock,
                   Storage, thunks::detail::TaggedVector, InputFuture>;

  return AllFuture(0, std::move(vec));
}

template <SomeFuture First, typename... Futures>
Future<std::tuple<traits::ValueOf<First>, traits::ValueOf<Futures>...>> auto
All(First f1, Futures... fs) {
  static_assert((SomeFuture<Futures> && ...));

  using Storage = thunks::detail::Tuple<First, Futures...>;
  using ValueType =
      std::tuple<traits::ValueOf<First>, traits::ValueOf<Futures>...>;

  using AllFuture =
      thunks::Join<true, /*OnHeap=*/true, ValueType, thunks::AllControlBlock,
                   Storage, thunks::detail::TaggedTuple, First, Futures...>;

  return AllFuture(0, std::move(f1), std::move(fs)...);
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

  using Storage = thunks::detail::Vector<InputFuture>;
  using ValueType = std::vector<traits::ValueOf<InputFuture>>;

  using AllFuture =
      thunks::Join<true, /*OnHeap=*/false, ValueType, thunks::AllControlBlock,
                   Storage, thunks::detail::TaggedVector, InputFuture>;

  return AllFuture(0, std::move(vec));
}

template <traits::Cancellable First, traits::Cancellable... Futures>
Future<std::tuple<traits::ValueOf<First>, traits::ValueOf<Futures>...>> auto
All(First f1, Futures... fs) {
  static_assert((SomeFuture<Futures> && ...));

  using Storage = thunks::detail::Tuple<First, Futures...>;
  using ValueType =
      std::tuple<traits::ValueOf<First>, traits::ValueOf<Futures>...>;

  using AllFuture =
      thunks::Join<true, /*OnHeap=*/false, ValueType, thunks::AllControlBlock,
                   Storage, thunks::detail::TaggedTuple, First, Futures...>;

  return AllFuture(0, std::move(f1), std::move(fs)...);
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
