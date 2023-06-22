#include <weave/executors/tp/fast/worker.hpp>
#include <weave/executors/tp/fast/thread_pool.hpp>

#include <wheels/core/panic.hpp>

namespace weave::executors::tp::fast {

void Worker::Push(Task* task, SchedulerHint hint) {
  switch (hint) {
    case SchedulerHint::Next:
      if constexpr (!kDisbaleLifoInteraction) {
        PushToLifoSlot(task);
        break;
      }

    case SchedulerHint::UpToYou:
      // overflow handling is delegated to PushToLocalQueue
      PushToLocalQueue(task);
      break;

    case SchedulerHint::Last:
      // Yielded task
      host_.global_tasks_.Push(task, prod_token_);
      break;

    default:
      WHEELS_PANIC("Unknown Scheduler hint!\n");
  };
}

void Worker::PushToLifoSlot(Task* task) {
  Task* former_lifo = nullptr;

  // success: task is pushed to consumers
  // fail: someone had stolen task from us
  while (!lifo_slot_.compare_exchange_weak(former_lifo, task,
                                           std::memory_order::release,
                                           std::memory_order::acquire)) {
    ;  // Backoff
  }

  // if we've taken the valid task off the lifo slot, we send it into the local
  // queue
  if (former_lifo != nullptr) {
    if constexpr (kCollectMetrics) {
      metrics_.times_discarded_lifo_slot_++;
    }

    PushToLocalQueue(former_lifo);
  }
}

// true if fast path
void Worker::PushToLocalQueue(Task* task) {
  if (!local_tasks_.TryPush(task)) {
    if constexpr (kCollectMetrics) {
      metrics_.times_offloaded_to_global_queue_++;
    }

    // we have overflow
    std::array<Task*, kLocalQueueCapacity / 2 + 1> overflow{};

    // try grab overflow
    size_t num_grabbed =
        local_tasks_.Grab({overflow.begin(), overflow.end() - 1});

    if (num_grabbed == 0) {
      // there is a chance that other workers have stolen tasks after we failed
      // to push but before we grabbed any tasks this implies that TryPush now
      // must succeed

      local_tasks_.TryPush(task);
      return;
    }

    // record task itself
    overflow[num_grabbed] = task;

    // Offload linked list
    OffloadTasksToGlobalQueue(overflow, num_grabbed + 1);
  }
}

void Worker::OffloadTasksToGlobalQueue(std::span<Task*> overflow,
                                       size_t valid_num) {
  host_.global_tasks_.Append({overflow.begin(), overflow.begin() + valid_num},
                             prod_token_);
}

}  // namespace weave::executors::tp::fast