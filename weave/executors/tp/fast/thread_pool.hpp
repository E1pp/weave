#pragma once

#include <weave/executors/executor.hpp>

#include <weave/executors/tp/fast/queues/global_queue.hpp>
#include <weave/executors/tp/fast/worker.hpp>
#include <weave/executors/tp/fast/coordinator.hpp>
#include <weave/executors/tp/fast/metrics.hpp>
#include <weave/executors/tp/fast/runner.hpp>

#include <weave/threads/blocking/work_count.hpp>

// random_device
#include <twist/ed/stdlike/random.hpp>

#include <deque>

namespace weave::executors::tp::fast {

// Scalable work-stealing scheduler for short-lived tasks

class ThreadPool : public IExecutor {
  friend class Worker;
  friend class Coordinator;

 public:
  explicit ThreadPool(const size_t threads);
  ~ThreadPool();

  // Non-copyable
  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;

  void Start();

  // IExecutor
  void Submit(Task*, SchedulerHint) override;

  void WaitIdle() {
    work_count_.Done(1);
    work_count_.Wait();
    work_count_.Add(1);
  }

  void Stop();

  // After Stop
  Logger::Metrics Metrics();

  static ThreadPool* Current();

  void SetRunner(IRunner& runner) {
    runner_ = &runner;
  }

  Logger* GetLogger(){
    #if !defined(__WEAVE_REALTIME__)
      WHEELS_PANIC("Real-time Logging is not supported. Please turn flag WEAVE_REALTIME_METRICS on.");
    #endif
    
    return &logger_;
  }

 private:
  // tries to wake up workers until
  // a) Worker wakes up
  // b) ParkingLot is empty
  // c) Some worker starts spinning
  void TryWakeWorkers() {
    while (bool keep_trying = !coordinator_.TryWakeWorker()) {
      ;  // Backoff
    }
  }

  IRunner& Runner() {
    return *runner_;
  }

 private:
  // used for constexpr kind of thing
  const size_t threads_;

  std::deque<Worker> workers_{};
  IRunner* runner_;

  Coordinator coordinator_;

  GlobalQueue global_tasks_;

  twist::ed::stdlike::random_device random_;

  twist::ed::stdlike::atomic<bool> stopped_{false};

  threads::blocking::WorkCount work_count_;

  Logger logger_;
};

}  // namespace weave::executors::tp::fast
