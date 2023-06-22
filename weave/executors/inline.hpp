#pragma once

#include <weave/executors/executor.hpp>

namespace weave::executors {

// Executes task immediately on the current thread

IExecutor& Inline();

}  // namespace weave::executors
