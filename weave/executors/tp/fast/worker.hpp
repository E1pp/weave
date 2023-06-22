#pragma once

#include <weave/executors/task.hpp>
#include <weave/executors/hint.hpp>

#include <weave/executors/tp/fast/queues/work_stealing_queue.hpp>
#include <weave/executors/tp/fast/coordinator.hpp>
#include <weave/executors/tp/fast/metrics.hpp>
#include <weave/executors/tp/fast/picker.hpp>
#include <weave/executors/tp/fast/task_flags.hpp>

#include <twist/ed/stdlike/atomic.hpp>
#include <twist/ed/stdlike/thread.hpp>

#include <wheels/intrusive/list.hpp>

#include <cstdlib>
#include <optional>
#include <random>
#include <span>

namespace weave::executors::tp::fast {

class ThreadPool;

///////////////////////////////////////////////////////////////////

class Worker : public wheels::IntrusiveListNode<Worker>,
               private IPicker {
  friend class Coordinator;

 private:
#if !defined(TWIST_FAULTY)
  static const size_t kLocalQueueCapacity = 256;
#else
  static const size_t kLocalQueueCapacity = 17;
#endif

  static const size_t kMaxLifoStreak =
      std::min(kVyukovGQueue, kLocalQueueCapacity) / 2;

 public:
  Worker(ThreadPool& host, size_t index);

  void Start();

  void Join();

  // Single producer
  void Push(Task*, SchedulerHint);

  // Steal from this worker
  size_t StealTasks(std::span<Task*> out_buffer);

  // Wake parked worker
  bool TryWake();

  void Wake();

  static Worker* Current();

  WorkerMetrics Metrics() const {
    return metrics_;
  }

  ThreadPool& Host() const {
    return host_;
  }

  // debugging
  size_t Index() {
    return index_;
  }

  ~Worker() override = default;

 private:
  // Use in Push
  void PushToLifoSlot(Task* task);
  void PushToLocalQueue(Task* task);

  // Use in PushToLocalQueue
  void OffloadTasksToGlobalQueue(std::span<Task*>, size_t);

  // Use in TryPickTask
  Task* TryGrabTasksFromGlobalQueue();
  Task* TryPickTaskFromLifoSlot();
  Task* TryPickTaskFromLocalQueueFast();
  Task* TryPickTaskFromLocalQueueSlow();
  Task* TryStealTasks();

  // Use in TryStealTasks
  Task* TryStealTaskIter();

  // Use in PickTask
  Task* TryPickTask();
  Task* TryPickTaskBeforePark();

  // Or park thread
  Task* PickTask() override;
  bool StopRequested() const override;

  // Run Loop
  void Work();

 private:
  ThreadPool& host_;
  const size_t index_;

  // Worker thread
  std::optional<twist::ed::stdlike::thread> thread_;

  // Scheduling iteration
  size_t iter_ = 0;
  size_t lifo_streak_ = 0;

  // Local queue
  WorkStealingQueue<kLocalQueueCapacity> local_tasks_;

  // LIFO slot
  twist::ed::stdlike::atomic<Task*> lifo_slot_{nullptr};

  // random generationc
  std::mt19937_64 twister_;
  std::vector<int> indeces_;

  // Parking lot & other coordination
  twist::ed::stdlike::atomic<uint32_t> wakeups_{0};
  twist::ed::stdlike::atomic<bool> idle_{false};

  WorkerMetrics metrics_;

  moodycamel::ConsumerToken cons_token_;
  moodycamel::ProducerToken prod_token_;
};

}  // namespace weave::executors::tp::fast
