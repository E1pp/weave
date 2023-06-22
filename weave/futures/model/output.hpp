#pragma once

#include <weave/result/types/result.hpp>

#include <weave/futures/model/context.hpp>

namespace weave::futures {

template <typename T>
struct Output {
  Result<T> result;
  Context context;
};

}  // namespace weave::futures
