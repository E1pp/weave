#pragma once

#include <weave/futures/model/consumer.hpp>

#include <concepts>
#include <type_traits>

namespace weave::futures {

template <typename F>
concept Thunk = std::is_nothrow_move_constructible_v<F> && !std::copyable<F> &&
                requires(F) {
  typename F::ValueType;
};

}  // namespace weave::futures