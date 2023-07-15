#pragma once

#include <weave/futures/old_syntax/pipe.hpp>

#include <weave/futures/thunks/combine/seq/flatten.hpp>

#include <weave/futures/old_traits/value_of.hpp>

#include <optional>

namespace weave::futures {

namespace pipe {

struct [[nodiscard]] Flatten {
  template <SomeFuture InputFuture>
  Future<traits::ValueOf<traits::ValueOf<InputFuture>>> auto Pipe(
      InputFuture f) {
    return futures::thunks::Flattenned(std::move(f));
  }
};

}  // namespace pipe

// Future<Future<T>> -> Future<T>

inline auto Flatten() {
  return pipe::Flatten{};
}

}  // namespace weave::futures
