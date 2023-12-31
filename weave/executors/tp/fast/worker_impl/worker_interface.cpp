#include <weave/executors/tp/fast/worker.hpp>
#include <weave/executors/tp/fast/thread_pool.hpp>

#include <twist/ed/local/ptr.hpp>
#include <twist/ed/wait/futex.hpp>

#include <wheels/core/panic.hpp>

#include <chrono>
#include <thread>

namespace weave::executors::tp::fast {

///////////////////////////////////////////////////////////////////

TWISTED_THREAD_LOCAL_PTR(Worker, worker);

///////////////////////////////////////////////////////////////////

Worker::Worker(ThreadPool& host, size_t index, Logger::LoggerShard* shard)
    : host_(host),
      index_(index),
      twister_(host_.random_()),
      indices_(host_.threads_ - 1),
      logger_shard_(shard) {
  std::iota(indices_.begin(), indices_.end(), 1);
}

Worker* Worker::Current() {
  return worker;
}

void Worker::Start() {
  host_.work_count_.StealthAdd(1);

  thread_.emplace([this]() {
    Work();
  });
}

void Worker::Join() {
  Wake();

  host_.work_count_.StealthDone(1);

  thread_->join();
}

void Worker::Work() {
  worker = this;

  host_.Runner().RunnerRoutine(*this);
}

// Park/Wake right here

bool Worker::TryWake() {
  if (idle_.load()) {
    Wake();
    return true;
  }

  return false;
}

void Worker::Wake() {
  auto wake_key = twist::ed::futex::PrepareWake(wakeups_);
  wakeups_.fetch_add(
      1, std::memory_order::relaxed);  // never access shared data anyway nor we
                                       // branch on wakeups particular value ->
                                       // relaxed everywhere
  twist::ed::futex::WakeOne(wake_key);
}

}  // namespace weave::executors::tp::fast
