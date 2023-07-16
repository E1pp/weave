#pragma once

#include <weave/futures/thunks/make/contract.hpp>

#include <weave/futures/thunks/combine/seq/start.hpp>

#include <weave/futures/traits/is_lazy.hpp>

namespace weave::futures {

template <typename Future>
concept EagerFuture = traits::Eager<Future>;

template <typename T>
using ContractFuture = thunks::ContractFuture<T>;

template <SomeFuture Future>
using StartFuture = thunks::StartFuture<Future>;

}  // namespace weave::futures
