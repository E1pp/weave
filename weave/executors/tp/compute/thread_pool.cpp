#include <weave/executors/tp/compute/thread_pool.hpp>

#include <twist/ed/local/ptr.hpp>

#include <wheels/core/panic.hpp>

namespace weave::executors::tp::compute {

static twist::ed::ThreadLocalPtr<ThreadPool> pool;

ThreadPool::ThreadPool(size_t num_threads)
    : num_threads_(num_threads) {
}

void ThreadPool::Start() {
  for (size_t i = 0; i < num_threads_; i++) {
    workers_.emplace_back([this]() {
      Worker();
    });
  }
}

void ThreadPool::Worker() noexcept {
  pool = this;

  while (Task* next = tasks_.Take()) {
    next->Run();

    incomplete_tasks_.Done();
  }
}

ThreadPool::~ThreadPool() {
  assert(workers_.empty());
}

void ThreadPool::Submit(Task* task, SchedulerHint) {
  incomplete_tasks_.Add(1);
  tasks_.Put(task);
}

ThreadPool* ThreadPool::Current() {
  return pool;
}

void ThreadPool::WaitIdle() {
  incomplete_tasks_.Wait();
}

void ThreadPool::Stop() {
  tasks_.Close();

  for (auto& thread : workers_) {
    thread.join();
  }

  workers_.clear();
}

}  // namespace weave::executors::tp::compute
