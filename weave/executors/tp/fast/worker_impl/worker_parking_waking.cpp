#include <weave/executors/tp/fast/worker.hpp>
#include <weave/executors/tp/fast/thread_pool.hpp>

namespace weave::executors::tp::fast {

bool Worker::StopRequested() const {
  return host_.stopped_.load(std::memory_order::relaxed);
}

// big loop
Task* Worker::PickTask() {
  iter_++;

  // global-lifo-local
  if (Task* task = TryPickTask()) {
    return task;
  }

  while (!StopRequested()) {
    // spinning routine
    if (host_.coordinator_.TryStartSpinning()) {
      Task* task = nullptr;

      if ((task = TryStealTasks()) == nullptr) {
        task = TryGrabTasksFromGlobalQueue();
      }

      if (host_.coordinator_.StopSpinning() && task != nullptr) {
        host_.TryWakeWorkers();
      }

      if (task != nullptr) {
        return task;
      }
    }

    {  // parking phase
      auto old = host_.coordinator_.AnnouncePark();

      // Double-check before parking
      if (Task* task = TryPickTaskBeforePark()) {
        host_.coordinator_.CancelPark();
        host_.TryWakeWorkers();
        return task;
      }

      if (StopRequested()) {
        host_.coordinator_.CancelPark();
        return nullptr;
      }

      host_.work_count_.Done(1);

      host_.coordinator_.TryParkMe(old);

      host_.work_count_.Add(1);
    }
  }

  return nullptr;
}

Task* Worker::TryPickTaskBeforePark() {
  if (Task* task = TryPickTaskFromLocalQueueSlow(); task != nullptr) {
    return task;
  }

  if (Task* task = TryStealTaskIter(); task != nullptr) {
    return task;
  }

  return nullptr;
}

}  // namespace weave::executors::tp::fast
