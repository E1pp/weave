#pragma once

#include <weave/executors/executor.hpp>

#include <weave/support/unbounded_blocking_queue.hpp>

#include <weave/threads/lockfull/wait_group.hpp>

#include <twist/ed/stdlike/thread.hpp>

#include <utility>
#include <vector>

namespace weave::executors::tp::compute {

// Thread pool for independent CPU-bound tasks
// Fixed pool of worker threads + shared unbounded blocking queue

class ThreadPool : public IExecutor {
 public:
  explicit ThreadPool(size_t threads);
  ~ThreadPool();

  // Non-copyable
  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;

  // Non-movable
  ThreadPool(ThreadPool&&) = delete;
  ThreadPool& operator=(ThreadPool&&) = delete;

  void Start();

  // IExecutor
  void Submit(Task*, SchedulerHint) override;

  static ThreadPool* Current();

  void WaitIdle();

  void Stop();

 private:
  void Worker() noexcept;

 private:
  // runtime core part: task queue, thread vector, threads number
  const size_t num_threads_;
  std::vector<twist::ed::stdlike::thread> workers_;
  support::UnboundedBlockingQueue<Task> tasks_;

  // WaitIdle
  threads::lockfull::WaitGroup incomplete_tasks_;
};

}  // namespace weave::executors::tp::compute
