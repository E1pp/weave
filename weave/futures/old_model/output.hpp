#pragma once

#include <weave/result/types/result.hpp>

#include <weave/futures/old_model/context.hpp>

namespace weave::futures {

template <typename T>
struct Output {
  Result<T> result;
  Context context;
};

}  // namespace weave::futures
