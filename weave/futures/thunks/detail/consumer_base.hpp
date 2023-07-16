#pragma once

#include <weave/futures/model/consumer.hpp>

namespace weave::futures::thunks::detail {

template <typename T, typename Derived>
struct ConsumerBase {
  ConsumerBase() {
    static_assert(TrimmedConsumer<Derived, T>);
  }

  void Complete(Result<T> r) {
    this->Complete({std::move(r), Context{}});
  }

  void Complete(Output<T> o) {
    static_cast<Derived>(this)->Consume(std::move(o));
  }
};

}  // namespace weave::futures::thunks::detail