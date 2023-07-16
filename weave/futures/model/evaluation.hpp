#pragma once

#include <type_traits>
#include <weave/futures/model/consumer.hpp>
#include <weave/futures/model/thunk.hpp>

#include <concepts>

namespace weave::futures {

template <typename E, typename F, typename C>
concept Evaluation =
    !std::movable<E> && !std::copyable<E> && requires(E eval, F fut, C& cons) {
  E(std::move(fut), cons);
  eval.Start();
};

template <typename C, typename F> // ProducerFor<F, C> == true
using EvaluationType = decltype(std::declval<std::add_rvalue_reference_t<F>>().Force(std::declval<std::add_lvalue_reference_t<C>>()));

}  // namespace weave::futures