#pragma once

#include <weave/fibers/core/handle.hpp>

#include <function2/function2.hpp>

namespace weave::fibers {

using Awaiter = fu2::function_view<FiberHandle(FiberHandle)>;

}  // namespace weave::fibers