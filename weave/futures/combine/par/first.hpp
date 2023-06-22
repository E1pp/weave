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

  auto* block =
      new thunks::FirstControlBlock<true, thunks::detail::Vector, InputFuture>(
          vec.size(), std::move(vec));

  return futures::thunks::Join(block);
}

template <SomeFuture FirstFuture, typename... Futures>
Future<traits::ValueOf<FirstFuture>> auto First(FirstFuture f1, Futures... fs) {
  static_assert(
      (std::is_same_v<traits::ValueOf<FirstFuture>, traits::ValueOf<Futures>> &&
       ...));

  auto* block = new thunks::FirstControlBlock<true, thunks::detail::Tuple,
                                              FirstFuture, Futures...>(
      1 + sizeof...(Futures), std::move(f1), std::move(fs)...);

  return futures::thunks::Join(block);
}

template <SomeFuture FirstFuture>
Future<traits::ValueOf<FirstFuture>> auto First(FirstFuture f1) {
  return std::move(f1);
}

// Doesn't allocate

// Actually vector version does allocate memory for outputs since it collects
// them in a vector But no_alloc versions do not allocate memory for input
// futures themselves

//////////////////////////////////////////////////////////////////////////////

namespace no_alloc {

template <traits::Cancellable InputFuture>
Future<traits::ValueOf<InputFuture>> auto First(std::vector<InputFuture> vec) {
  WHEELS_VERIFY(!vec.empty(), "Sending empty vector!");

  auto block =
      thunks::FirstControlBlock<false, thunks::detail::Vector, InputFuture>(
          vec.size(), std::move(vec));

  return futures::thunks::JoinOnStack(std::move(block));
}

template <traits::Cancellable FirstFuture, traits::Cancellable... Futures>
Future<traits::ValueOf<FirstFuture>> auto First(FirstFuture f1, Futures... fs) {
  static_assert(
      (std::is_same_v<traits::ValueOf<FirstFuture>, traits::ValueOf<Futures>> &&
       ...));

  thunks::FirstControlBlock<false, thunks::detail::Tuple, FirstFuture,
                            Futures...>
      block(1 + sizeof...(Futures), std::move(f1), std::move(fs)...);

  return futures::thunks::JoinOnStack(std::move(block));
}

template <SomeFuture FirstFuture>
Future<traits::ValueOf<FirstFuture>> auto First(FirstFuture f1) {
  return std::move(f1);
}

}  // namespace no_alloc

}  // namespace weave::futures
