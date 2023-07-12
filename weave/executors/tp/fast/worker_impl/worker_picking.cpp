#include <weave/executors/tp/fast/worker.hpp>
#include <weave/executors/tp/fast/thread_pool.hpp>

namespace weave::executors::tp::fast {

Task* Worker::TryPickTask() {
  // * [%61] Global queue +
  // * LIFO slot +
  // * Local queue +
  // * Global queue +

  Task* task;

  // check global queue occasionally
  if (iter_ % kVyukovGQueue == 0) {
    if ((task = TryGrabTasksFromGlobalQueue()) != nullptr) {
      logger_shard_->Increment("Launched from global queue", 1);

      return task;
    }
  }

  // check LIFO slot
  if ((task = TryPickTaskFromLifoSlot()) != nullptr) {
    logger_shard_->Increment("Launched from lifo" ,1);

    return task;
  }

  task = TryPickTaskFromLocalQueueFast();

  return task;
}

Task* Worker::TryGrabTasksFromGlobalQueue() {
  Task* next = host_.global_tasks_.TryPop();

  if (next != nullptr && TaskFlags::IsSet(next->flags, TaskFlags::External)) {
    TaskFlags::Reset(next->flags, TaskFlags::External);
    host_.work_count_.StealthDone(1);
  }

  return next;
}

Task* Worker::TryPickTaskFromLifoSlot() {
  // sync with producer of lifo_slot_
  Task* next = lifo_slot_.exchange(nullptr, std::memory_order::acquire);

  // check the streak of lifo runs
  if (next != nullptr) {
    lifo_streak_++;
  }

  // time to stop
  if (lifo_streak_ == kMaxLifoStreak) {
    // if some worker steals our entire local queue
    // AND global queue is empty, we will not have anywhere
    // to reset the counter in this TryPickTask iteration
    lifo_streak_ = 0;

    logger_shard_->Increment("Discarded lifo_slots", 1);

    // push task from lifo_slot to local queue
    PushToLocalQueue(next);
    // host_.TryWakeWorkers();

    return nullptr;
  }

  return next;
}

Task* Worker::TryPickTaskFromLocalQueueFast() {
  Task* task = local_tasks_.TryPop();

  // hot path
  if (task != nullptr) {
    // reset lifo streak and return task
    lifo_streak_ = 0;

    logger_shard_->Increment("Launched from local queue", 1);

    return task;
  }

  return TryPickTaskFromLocalQueueSlow();
}

Task* Worker::TryPickTaskFromLocalQueueSlow() {
  // try to restock from GlobalQueue
  Task* task = nullptr;

  std::array<Task*, kLocalQueueCapacity> buffer{};

  // write into the buffer from GlobalQueue
  size_t num_taken =
      host_.global_tasks_.Grab(buffer, host_.threads_);
  if (num_taken != 0) {
    // Work count processing
    size_t external_tasks = 0;
    for (size_t i = 0; i < num_taken; i++) {
      if (TaskFlags::IsSet(buffer[i]->flags, TaskFlags::External)) {
        TaskFlags::Reset(buffer[i]->flags, TaskFlags::External);
        external_tasks++;
      }
    }
    host_.work_count_.StealthDone(external_tasks);

    // we found task to return -> reset lifo streak
    lifo_streak_ = 0;

    logger_shard_->Increment("Launched from global queue", 1);

    // grab first task for yourself and push the rest into the empty local queue
    task = buffer[0];

    local_tasks_.PushMany({buffer.begin() + 1, buffer.begin() + num_taken});
  }

  // either returns nullptr if restocking failed or a valid ptr from buffer[0]
  return task;
}

}  // namespace weave::executors::tp::fast