#pragma once

#include <weave/futures/thunks/combine/seq/anyway.hpp>

#include <weave/futures/syntax/pipe.hpp>

#include <optional>

#include <type_traits>

namespace weave::futures {

namespace pipe {

template <typename F>
struct Anyway {
  F fun;

  explicit Anyway(F f)
      : fun(std::move(f)) {
  }

  template <SomeFuture InputFuture>
  Future<traits::ValueOf<InputFuture>> auto Pipe(InputFuture f) {
    return thunks::Anyway(std::move(f), std::move(fun));
  }
};

}  // namespace pipe

// Future<T> -> (void -> Unit / void) -> Future<T>

template <typename F>
auto Anyway(F fun) {
  return pipe::Anyway{std::move(fun)};
}

}  // namespace weave::futures