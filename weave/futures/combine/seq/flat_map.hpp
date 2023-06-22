#pragma once

#include <weave/futures/syntax/pipe.hpp>

#include <weave/futures/traits/value_of.hpp>

#include <weave/futures/combine/seq/flatten.hpp>
#include <weave/futures/combine/seq/map.hpp>

#include <type_traits>

namespace weave::futures {

namespace pipe {

template <typename F>
struct [[nodiscard]] FlatMap {
  F fun;

  explicit FlatMap(F f)
      : fun(std::move(f)) {
  }

  template <typename T>
  using U = traits::ValueOf<std::invoke_result_t<F, T>>;

  template <SomeFuture InputFuture>
  Future<U<traits::ValueOf<InputFuture>>> auto Pipe(InputFuture f) {
    return std::move(f) | futures::Map(std::move(fun)) | futures::Flatten();
  }
};

}  // namespace pipe

// Future<T> -> (T -> Future<U>) -> Future<U>

template <typename F>
auto FlatMap(F fun) {
  return pipe::FlatMap{std::move(fun)};
}

}  // namespace weave::futures
