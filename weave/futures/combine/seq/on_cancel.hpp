#pragma once

#include <weave/futures/thunks/combine/seq/on_cancel.hpp>

#include <weave/futures/syntax/pipe.hpp>

#include <weave/result/complete/complete.hpp>

#include <optional>

#include <type_traits>

namespace weave::futures {

namespace pipe {

template <typename F>
struct OnCancel {
  F fun;

  explicit OnCancel(F f)
      : fun(std::move(f)) {
  }

  template <SomeFuture InputFuture>
  Future<traits::ValueOf<InputFuture>> auto Pipe(InputFuture f) {
    return thunks::OnCancel(std::move(f), std::move(fun));
  }
};

}  // namespace pipe

// Future<T> -> (void -> Unit / void) -> Future<T>

template <typename F>
auto OnCancel(F fun) {
  return pipe::OnCancel{std::move(fun)};
}

}  // namespace weave::futures