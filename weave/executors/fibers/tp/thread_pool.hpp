#pragma once

#include <weave/executors/tp/fast/thread_pool.hpp>

#include <weave/executors/fibers/tp/fiber_runner.hpp>

namespace weave::executors::fibers {

class ThreadPool final : private runners::FiberRunner,
                         public executors::tp::fast::ThreadPool {
 public:
  explicit ThreadPool(size_t threads);

  // Non-copyable
  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;

  // Non-movable
  ThreadPool(ThreadPool&&) = delete;
  ThreadPool& operator=(ThreadPool&&) = delete;

  bool IRunFibers() override {
    return true;
  }

  ~ThreadPool() override = default;
};

}  // namespace weave::executors::fibers