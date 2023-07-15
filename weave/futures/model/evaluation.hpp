#pragma once

#include <weave/futures/model/consumer.hpp>
#include <weave/futures/model/thunk.hpp>

#include <concepts>

namespace weave::futures {

template <typename E, typename F, typename C>
concept Evaluation =
    !std::movable<E> && !std::copyable<E> && requires(F fut, C& cons) {
  E(std::move(fut), cons);
};

template <typename F, typename C>
concept ProducerFor = Thunk<F> && Consumer<C, typename F::ValueType> &&
    requires(F thunk, C& consumer) {
  std::move(thunk).Force(consumer)->Evaluation<F, C>;
};

}  // namespace weave::futures