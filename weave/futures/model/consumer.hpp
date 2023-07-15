#pragma once

#include <weave/cancel/token.hpp>

#include <weave/futures/model/output.hpp>

namespace weave::futures {

template <typename C, typename T>
concept Consumer = requires(C& consumer, Output<T> o, Result<T> r, Context ctx) {
  consumer.Complete(std::move(o));
  consumer.Complete(std::move(r));
  
  // Cancellation
  consumer.Cancel(ctx);
  {consumer.CancelToken()} -> std::same_as<cancel::Token>;
};

}  // namespace weave::futures
