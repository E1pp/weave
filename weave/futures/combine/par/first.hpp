#pragma once

#include <weave/futures/thunks/combine/par/join.hpp>

#include <weave/futures/thunks/combine/par/first/tuple.hpp>
#include <weave/futures/thunks/combine/par/first/vector.hpp>

namespace weave::futures {

// Allocates

//////////////////////////////////////////////////////////////////////////////

template <SomeFuture InputFuture>
Future<traits::ValueOf<InputFuture>> auto First(std::vector<InputFuture> vec) {
  WHEELS_VERIFY(!vec.empty(), "Sending empty vector!");

  using Storage = thunks::detail::Vector<InputFuture>;
  using ValueType = traits::ValueOf<InputFuture>;

  using FirstFuture =
      thunks::Join<true, /*OnHeap=*/true, ValueType, thunks::FirstControlBlock,
                   Storage, thunks::detail::TaggedVector, InputFuture>;

  return FirstFuture(0, std::move(vec));
}

template <SomeFuture FirstF, typename... Fs>
Future<traits::ValueOf<FirstF>> auto First(FirstF f1, Fs... fs) {
  static_assert(
      (std::is_same_v<traits::ValueOf<FirstF>, traits::ValueOf<Fs>> && ...));

  using Storage = thunks::detail::Tuple<FirstF, Fs...>;
  using ValueType = traits::ValueOf<FirstF>;

  using FirstFuture =
      thunks::Join<true, /*OnHeap=*/true, ValueType, thunks::FirstControlBlock,
                   Storage, thunks::detail::TaggedTuple, FirstF, Fs...>;

  return FirstFuture(0, std::move(f1), std::move(fs)...);
}

template <SomeFuture FirstFuture>
Future<traits::ValueOf<FirstFuture>> auto First(FirstFuture f1) {
  return std::move(f1);
}

// Doesn't allocate

//////////////////////////////////////////////////////////////////////////////

namespace no_alloc {

template <traits::Cancellable InputFuture>
Future<traits::ValueOf<InputFuture>> auto First(std::vector<InputFuture> vec) {
  WHEELS_VERIFY(!vec.empty(), "Sending empty vector!");

  using Storage = thunks::detail::Vector<InputFuture>;
  using ValueType = traits::ValueOf<InputFuture>;

  using FirstFuture =
      thunks::Join<true, /*OnHeap=*/false, ValueType, thunks::FirstControlBlock,
                   Storage, thunks::detail::TaggedVector, InputFuture>;

  return FirstFuture(0, std::move(vec));
}

template <traits::Cancellable FirstF, traits::Cancellable... Fs>
Future<traits::ValueOf<FirstF>> auto First(FirstF f1, Fs... fs) {
  static_assert(
      (std::is_same_v<traits::ValueOf<FirstF>, traits::ValueOf<Fs>> && ...));

  using Storage = thunks::detail::Tuple<FirstF, Fs...>;
  using ValueType = traits::ValueOf<FirstF>;

  using FirstFuture =
      thunks::Join<true, /*OnHeap=*/false, ValueType, thunks::FirstControlBlock,
                   Storage, thunks::detail::TaggedTuple, FirstF, Fs...>;

  return FirstFuture(0, std::move(f1), std::move(fs)...);
}

template <SomeFuture FirstFuture>
Future<traits::ValueOf<FirstFuture>> auto First(FirstFuture f1) {
  return std::move(f1);
}

}  // namespace no_alloc

}  // namespace weave::futures
