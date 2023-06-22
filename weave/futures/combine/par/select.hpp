#pragma once

#include <weave/futures/thunks/combine/par/join.hpp>

#include <weave/futures/thunks/combine/par/select/tuple.hpp>

namespace weave::futures {

// Allocates

//////////////////////////////////////////////////////////////////////////////

template <SomeFuture FirstFuture, typename... Futures>
Future<thunks::SelectedValue<FirstFuture, Futures...>> auto Select(
    FirstFuture f1, Futures... fs) {
  auto* block = new thunks::SelectControlBlock<true, thunks::detail::Tuple,
                                               FirstFuture, Futures...>(
      1 + sizeof...(Futures), std::move(f1), std::move(fs)...);

  return futures::thunks::Join(block);
}

template <SomeFuture FirstFuture>
Future<traits::ValueOf<FirstFuture>> auto Select(FirstFuture f1) {
  return std::move(f1);
}

// Doesn't allocate

//////////////////////////////////////////////////////////////////////////////

namespace no_alloc {

template <traits::Cancellable FirstFuture, traits::Cancellable... Futures>
Future<thunks::SelectedValue<FirstFuture, Futures...>> auto Select(
    FirstFuture f1, Futures... fs) {
  thunks::SelectControlBlock<false, thunks::detail::Tuple, FirstFuture,
                             Futures...>
      block(1 + sizeof...(Futures), std::move(f1), std::move(fs)...);

  return futures::thunks::JoinOnStack(std::move(block));
}

template <SomeFuture FirstFuture>
Future<traits::ValueOf<FirstFuture>> auto Select(FirstFuture f1) {
  return std::move(f1);
}

}  // namespace no_alloc

}  // namespace weave::futures