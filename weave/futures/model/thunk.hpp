#pragma once

#include <weave/futures/model/consumer.hpp>

#include <concepts>
#include <type_traits>

namespace weave::futures {

template <typename F>
concept Thunk = requires(F) {
  typename F::ValueType;
};

}  // namespace weave::futures