#pragma once

#include <weave/futures/syntax/pipe.hpp>

#include <weave/futures/thunks/combine/seq/fork.hpp>

#include <weave/futures/traits/value_of.hpp>

#include <optional>
#include <type_traits>

namespace weave::futures {

namespace pipe {

template <size_t NumTines>
struct [[nodiscard]] Fork {
  template <SomeFuture InputFuture>
  requires std::is_copy_constructible_v<typename InputFuture::ValueType>
  auto Pipe(InputFuture f) {
    auto* forker = new thunks::Forker<NumTines, decltype(f)>(std::move(f));

    return forker->MakeTines();
  }
};

template <>
struct [[nodiscard]] Fork<0> {};

template <>
struct [[nodiscard]] Fork<1> {
  template <SomeFuture InputFuture>
  auto Pipe(InputFuture f) {
    return std::move(f);
  }
};

}  // namespace pipe

// Future<T> -> Future<T> x NumTines

template <size_t NumTines>
inline auto Fork() {
  return pipe::Fork<NumTines>{};
}

}  // namespace weave::futures
