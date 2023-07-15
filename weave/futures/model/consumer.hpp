#pragma once

#include <weave/futures/model/output.hpp>

namespace weave::futures {

template <typename C, typename T>
concept Consumer = requires(C& consumer, Output<T> o, Result<T> r) {
  consumer.Complete(std::move(o));
  consumer.Complete(std::move(r));
  // consumer->CancelToken()
};

}  // namespace weave::futures
