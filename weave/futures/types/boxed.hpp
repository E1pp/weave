#pragma once

#include <weave/futures/types/naked.hpp>

#include <weave/futures/thunks/combine/seq/box.hpp>

namespace weave::futures {

template <typename T>
using BoxedFuture = thunks::Boxed<T>;

}  // namespace weave::futures
