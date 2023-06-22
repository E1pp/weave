#pragma once

#include <weave/executors/fibers/thread_pool.hpp>

namespace weave::executors {

// Default thread pool implementation
using ThreadPool = fibers::ThreadPool;

}  // namespace weave::executors
