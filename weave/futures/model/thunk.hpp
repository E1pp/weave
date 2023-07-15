#pragma once

#include <__concepts/constructible.h>
#include <weave/futures/model/consumer.hpp>

#include <concepts>

namespace weave::futures {

template <typename F>
concept Thunk = std::move_constructible<F> && !std::copyable<F> && requires(F) {
  typename F::ValueType;
};

}  // namespace weave::futures