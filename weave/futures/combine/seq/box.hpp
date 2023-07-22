#pragma once

#include <weave/futures/syntax/pipe.hpp>

#include <weave/futures/traits/value_of.hpp>

#include <weave/futures/types/boxed.hpp>

namespace weave::futures {

namespace pipe {

struct [[nodiscard]] Box {
  template <SomeFuture InputFuture>
  BoxedFuture<traits::ValueOf<InputFuture>> Pipe(InputFuture f) {
    return BoxedFuture<traits::ValueOf<InputFuture>>(std::move(f));
  }
};

struct [[nodiscard]] CBox {
  template <SomeFuture InputFuture>
  CBoxedFuture<traits::ValueOf<InputFuture>> Pipe(InputFuture f) {
    return CBoxedFuture<traits::ValueOf<InputFuture>>(std::move(f));
  }
};

}  // namespace pipe

// Future<T> -> BoxedFuture<T>

inline auto Box() {
  return pipe::Box{};
}

// Cancellable box
inline auto CBox() {
  return pipe::CBox{};
}

}  // namespace weave::futures
