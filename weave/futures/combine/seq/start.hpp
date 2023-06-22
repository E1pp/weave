#pragma once

#include <weave/futures/syntax/pipe.hpp>

#include <weave/futures/traits/value_of.hpp>

#include <weave/futures/types/eager.hpp>

namespace weave::futures {

namespace pipe {

struct [[nodiscard]] Start {
  template <SomeFuture InputFuture>
  StartedFuture<InputFuture> Pipe(InputFuture f) {
    return futures::thunks::StartedFuture(std::move(f));
  }
};

}  // namespace pipe

// Future<T> -> EagerFuture<T>

inline auto Start() {
  return pipe::Start{};
}

inline auto Force() {
  return pipe::Start{};
}

}  // namespace weave::futures
