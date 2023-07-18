#pragma once

#include <weave/futures/model/consumer.hpp>

#include <concepts>
#include <type_traits>

namespace weave::futures {

template <typename F>
concept UnrestrictedThunk = requires(F) {
  typename F::ValueType;
};

template <typename F>
concept Thunk = std::is_nothrow_move_constructible_v<F> && !std::copyable<F> &&
                UnrestrictedThunk<F>;

}  // namespace weave::futures