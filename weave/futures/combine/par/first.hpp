#pragma once

#include <weave/futures/thunks/combine/par/join.hpp>

#include <weave/futures/thunks/combine/par/first/tuple.hpp>
#include <weave/futures/thunks/combine/par/first/vector.hpp>

namespace weave::futures {

// Allocates

//////////////////////////////////////////////////////////////////////////////

template <SomeFuture InputFuture>
Future<traits::ValueOf<InputFuture>> auto First(std::vector<InputFuture> vec){
  const size_t size = vec.size();

  WHEELS_VERIFY(size != 0, "Sending empty vector!");

  using Storage = thunks::detail::Vector<InputFuture>;
  using ValueType = traits::ValueOf<InputFuture>;

  using FirstFuture = futures::thunks::Join<true, ValueType, thunks::FirstControlBlock, Storage, thunks::detail::TaggedVector, InputFuture>;

  return FirstFuture(size, std::move(vec));
}

template <SomeFuture FirstF, typename... Fs>
Future<traits::ValueOf<FirstF>> auto First(FirstF f1, Fs... fs) {
  static_assert(
      (std::is_same_v<traits::ValueOf<FirstF>, traits::ValueOf<Fs>> && ...));

  using Storage = thunks::detail::Tuple<FirstF, Fs...>;
  using ValueType = traits::ValueOf<FirstF>;

  using FirstFuture = futures::thunks::Join<true, ValueType, thunks::FirstControlBlock, Storage, thunks::detail::TaggedTuple, FirstF, Fs...>;

  const size_t size = 1 + sizeof...(Fs);

  return FirstFuture(size, std::move(f1), std::move(fs)...);
}

template <SomeFuture FirstFuture>
Future<traits::ValueOf<FirstFuture>> auto First(FirstFuture f1) {
  return std::move(f1);
}

// Doesn't allocate

//////////////////////////////////////////////////////////////////////////////

// namespace no_alloc {

// template <traits::Cancellable InputFuture>
// Future<traits::ValueOf<InputFuture>> auto First(std::vector<InputFuture> vec)
// {
//   WHEELS_VERIFY(!vec.empty(), "Sending empty vector!");

//   auto block =
//       thunks::FirstControlBlock<false, thunks::detail::Vector, InputFuture>(
//           vec.size(), std::move(vec));

//   return futures::thunks::JoinOnStack(std::move(block));
// }

// template <traits::Cancellable FirstFuture, traits::Cancellable... Futures>
// Future<traits::ValueOf<FirstFuture>> auto First(FirstFuture f1, Futures...
// fs) {
//   static_assert(
//       (std::is_same_v<traits::ValueOf<FirstFuture>, traits::ValueOf<Futures>>
//       &&
//        ...));

//   thunks::FirstControlBlock<false, thunks::detail::Tuple, FirstFuture,
//                             Futures...>
//       block(1 + sizeof...(Futures), std::move(f1), std::move(fs)...);

//   return futures::thunks::JoinOnStack(std::move(block));
// }

// template <SomeFuture FirstFuture>
// Future<traits::ValueOf<FirstFuture>> auto First(FirstFuture f1) {
//   return std::move(f1);
// }

// }  // namespace no_alloc

}  // namespace weave::futures
