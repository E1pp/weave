#include <weave/executors/tp/fast/task_flags.hpp>

#include <weave/executors/tp/fast/thread_pool.hpp>
#include <weave/executors/tp/fast/thread_runner.hpp>

namespace weave::executors::tp::fast {

ThreadPool::ThreadPool(const size_t threads)
    : threads_(threads),
      runner_(&runners::ThreadRunner::Instance()),
      logger_(kMetrics, threads) {
  // create workers

  for (size_t i = 0; i < threads; ++i) {
    workers_.emplace_back(*this, i, logger_.MakeShard(i));
  }
}

void ThreadPool::Start() {
  work_count_.Add(1);

  for (auto& worker : workers_) {
    worker.Start();
  }
}

ThreadPool::~ThreadPool() {
  assert(workers_.empty());
}

void ThreadPool::Submit(Task* task, SchedulerHint hint) {
  // load source of the submit call
  Worker* sender = Worker::Current();

  // sender is from the outside of this scheduler
  if (sender == nullptr || &(sender->Host()) != this) {
    TaskFlags::SetBits(task->flags, TaskFlags::External);

    work_count_.StealthAdd(1);
    global_tasks_.Push(task);

    TryWakeWorkers();

    return;
  }

  // sender is an actual worker
  // -> delegate Submit to sender
  sender->Push(task, hint);

  TryWakeWorkers();
}

void ThreadPool::Stop() {
  // declare that the work is over
  stopped_.store(true,
                 std::memory_order::relaxed);  // we sync on thread::join
                                               // anyway so just relaxed

  // join workers
  for (auto& worker : workers_) {
    worker.Join();
  }

  // clear workers_
  workers_.clear();
}

Logger::Metrics ThreadPool::Metrics() {
  return logger_.GatherMetrics();
}

ThreadPool* ThreadPool::Current() {
  auto* worker = Worker::Current();
  return worker == nullptr ? nullptr : &worker->Host();
}

}  // namespace weave::executors::tp::fast
