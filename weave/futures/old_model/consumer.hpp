#pragma once

#include <weave/cancel/token.hpp>

#include <weave/futures/old_model/output.hpp>

namespace weave::futures {

// Represents future combinator / blocked thread / suspended stackful fiber
// or stackless coroutine

template <typename T>
struct IConsumer {
  virtual ~IConsumer() = default;

  // Invoked by producer

  void Complete(Result<T> r) noexcept {
    Consume({std::move(r), Context{}});
  }

  void Complete(Output<T> o) noexcept {
    Consume(std::move(o));
  }

  // Overriden by consumer
  virtual void Consume(Output<T>) noexcept = 0;

  virtual void Cancel(Context) noexcept = 0;

  virtual cancel::Token CancelToken() = 0;
};

}  // namespace weave::futures
