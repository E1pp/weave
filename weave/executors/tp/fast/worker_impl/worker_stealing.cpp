#include <weave/executors/tp/fast/worker.hpp>
#include <weave/executors/tp/fast/thread_pool.hpp>

#include <twist/ed/stdlike/thread.hpp>

namespace weave::executors::tp::fast {

// here are the functions which are impl of work stealing
// they don't do any checks are expected to be coordinated by someone

Task* Worker::TryStealTasks() {
  // another proc.go ispiration
  const size_t steal_attempts = 4;

  Task* task = nullptr;

  if constexpr (kCollectMetrics) {
    metrics_.times_tried_stealing_++;
  }

  // randomise sequence for every iter
  std::shuffle(indeces_.begin(), indeces_.end(), twister_);

  for (size_t i = 0; i < steal_attempts && task == nullptr; i++) {
    if constexpr (kCollectMetrics) {
      metrics_.times_we_iterated_stealing_++;
    }

    task = TryStealTaskIter();
  }

  return task;
}

Task* Worker::TryStealTaskIter() {
  Task* task = nullptr;
  std::array<Task*, kLocalQueueCapacity / 2> buffer{};
  const size_t max_index = host_.threads_;

  for (auto offset : indeces_) {
    // we try to steal from worker and if we steal something we assign it to
    // task

    size_t num_stolen =
        host_.workers_[(index_ + offset) % (max_index)].StealTasks(buffer);

    // we have stolen something
    if (num_stolen != 0) {
      task = buffer[0];

      lifo_streak_ = 0;
      // push surplus into local queue
      local_tasks_.PushMany({buffer.begin() + 1, buffer.begin() + num_stolen});

      break;
    }
  }

  return task;
}

// try steal from LIFO then try steal from local queue anyway
size_t Worker::StealTasks(std::span<Task*> out_buffer) {
  // skip idle worker here
  if (idle_.load(std::memory_order::relaxed)) {
    return 0;
  }

  size_t offset = 0;

  if constexpr (!kDisbaleLifoInteraction && !kPreventLifoStealing) {
    // stealing lifo first
    Task* task = lifo_slot_.exchange(nullptr, std::memory_order::acquire);

    if (task != nullptr) {
      if constexpr (kCollectMetrics) {
        Worker::Current()->metrics_.times_stolen_from_lifo_slot_++;
      }

      out_buffer[0] = task;
      // move offset if lifo was stolen
      offset = 1;
    }
  }

  // consider letting owner launch it's task from lifo

  size_t stolen_from_local_queue =
      offset +
      local_tasks_.Grab({out_buffer.begin() + offset, out_buffer.end()});

  if constexpr (kCollectMetrics) {
    if (stolen_from_local_queue != 0) {
      Worker::Current()->metrics_.tasks_stolen_from_local_queue_++;
    }
  }

  return stolen_from_local_queue;
}

}  // namespace weave::executors::tp::fast
