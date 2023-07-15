#pragma once

#include <weave/futures/thunks/combine/seq/on_success.hpp>

#include <weave/futures/old_syntax/pipe.hpp>

#include <optional>

#include <type_traits>

namespace weave::futures {

namespace pipe {

template <typename F>
struct OnSuccess {
  F fun;

  explicit OnSuccess(F f)
      : fun(std::move(f)) {
  }

  template <SomeFuture InputFuture>
  Future<traits::ValueOf<InputFuture>> auto Pipe(InputFuture f) {
    return thunks::OnSuccess(std::move(f), std::move(fun));
  }
};

}  // namespace pipe

// Future<T> -> (void -> Unit / void) -> Future<T>

template <typename F>
auto OnSuccess(F fun) {
  return pipe::OnSuccess{std::move(fun)};
}

}  // namespace weave::futures