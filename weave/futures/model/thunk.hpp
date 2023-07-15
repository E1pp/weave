#pragma once

#include <weave/futures/model/consumer.hpp>

#include <concepts>

namespace weave::futures {

template <typename F>
concept Thunk = std::movable<F> && !std::copyable<F> && requires(F ){
  typename F::ValueType;
};

} // namespace weave::futures