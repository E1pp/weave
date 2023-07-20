#pragma once

#include <weave/futures/model/consumer.hpp>
#include <weave/futures/model/thunk.hpp>

#include <concepts>

namespace weave::futures {

template <typename E, typename F, typename C>
concept Evaluation =
    !std::movable<E> && !std::copyable<E> && requires(E eval, F fut, C& cons) {
  eval.Start();
};

template <typename F, typename C>
concept ProducerFor = Thunk<F> && requires (F f, C& cons){
  {std::move(f).Force(cons)} -> Evaluation<F, C>;
};

// Technically ProducerFor is tautalogical here
// since Force checks Consumer<F::ValueType> anyway
template <typename C, ProducerFor<C> F>  
using EvaluationType =
    decltype(std::declval<std::add_rvalue_reference_t<F>>().Force(
        std::declval<std::add_lvalue_reference_t<C>>()));

}  // namespace weave::futures