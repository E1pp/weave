#pragma once

#include <weave/futures/thunks/combine/par/join.hpp>

#include <weave/futures/thunks/combine/par/select/tuple.hpp>

namespace weave::futures {

// Allocates

//////////////////////////////////////////////////////////////////////////////

template <SomeFuture FirstFuture, typename... Futures>
Future<thunks::SelectedValue<FirstFuture, Futures...>> auto Select(
    FirstFuture f1, Futures... fs) {
  
  using Storage = thunks::detail::Tuple<FirstFuture, Futures...>;
  using ValueType = thunks::SelectedValue<FirstFuture, Futures...>;

  using SelectFuture = thunks::Join<true, ValueType, thunks::SelectControlBlock, Storage, thunks::detail::TaggedTuple, FirstFuture, Futures...>;

  return SelectFuture(0, std::move(f1), std::move(fs)...); 
}

template <SomeFuture FirstFuture>
Future<traits::ValueOf<FirstFuture>> auto Select(FirstFuture f1) {
  return std::move(f1);
}

// Doesn't allocate

//////////////////////////////////////////////////////////////////////////////

// namespace no_alloc {

// template <traits::Cancellable FirstFuture, traits::Cancellable... Futures>
// Future<thunks::SelectedValue<FirstFuture, Futures...>> auto Select(
//     FirstFuture f1, Futures... fs) {
//   thunks::SelectControlBlock<false, thunks::detail::Tuple, FirstFuture,
//                              Futures...>
//       block(1 + sizeof...(Futures), std::move(f1), std::move(fs)...);

//   return futures::thunks::JoinOnStack(std::move(block));
// }

// template <SomeFuture FirstFuture>
// Future<traits::ValueOf<FirstFuture>> auto Select(FirstFuture f1) {
//   return std::move(f1);
// }

// }  // namespace no_alloc

}  // namespace weave::futures