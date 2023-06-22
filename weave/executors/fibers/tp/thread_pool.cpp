#include <weave/executors/fibers/tp/thread_pool.hpp>

namespace weave::executors::fibers {

ThreadPool::ThreadPool(size_t threads)
    : runners::FiberRunner(),
      executors::tp::fast::ThreadPool(threads) {
  SetRunner(*this);
}

}  // namespace weave::executors::fibers