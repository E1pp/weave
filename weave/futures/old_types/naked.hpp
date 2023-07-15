#pragma once

#include <weave/futures/old_model/thunk.hpp>

namespace weave::futures {

template <typename F>
concept SomeFuture = Thunk<F>;

template <typename F, typename T>
concept Future = SomeFuture<F> && std::same_as<typename F::ValueType, T>;

}  // namespace weave::futures
