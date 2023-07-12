#pragma once

#include <weave/executors/task.hpp>

#include <weave/threads/lockfull/stdlike/mutex.hpp>
#include <weave/threads/lockfull/spinlock.hpp>

#include <twist/ed/stdlike/mutex.hpp>

#include <wheels/intrusive/list.hpp>

#include <concurrentqueue.h>

#include <span>

namespace weave::executors::tp::fast {

class GlobalQueueBlockingImpl;

class GlobalQueueLockfreeImpl;

// Unbounded queue shared between workers
using GlobalQueue = GlobalQueueBlockingImpl;

// Check different mutexes
class GlobalQueueBlockingImpl {
 public:
  void Push(Task* item) {
    threads::lockfull::stdlike::LockGuard lock(mutex_);

    tasks_.PushBack(item);
    size_++;
  }

  void Push(Task* item, moodycamel::ProducerToken&) {
    threads::lockfull::stdlike::LockGuard lock(mutex_);

    tasks_.PushBack(item);
    size_++;
  }

  void Append(std::span<Task*> overflow, moodycamel::ProducerToken&) {
    // prepare a list to merge
    wheels::IntrusiveList<Task> overflow_l;

    for (auto node : overflow) {
      overflow_l.PushBack(node);
    }

    threads::lockfull::stdlike::LockGuard lock(mutex_);

    tasks_.Append(overflow_l);
    size_ += overflow.size();
  }

  // Returns nullptr if queue is empty
  Task* TryPop(moodycamel::ConsumerToken&) {
    threads::lockfull::stdlike::LockGuard lock(mutex_);

    Task* task;
    if ((task = tasks_.PopFront()) != nullptr) {
      size_--;
    }

    return task;
  }

  // Returns number of items in `out_buffer`
  size_t Grab(std::span<Task*> out_buffer, size_t workers,
              moodycamel::ConsumerToken&) {
    threads::lockfull::stdlike::LockGuard lock(mutex_);

    size_t num_to_grab = std::min(
        size_ < workers && size_ != 0 ? 1 : size_ / workers, out_buffer.size());

    for (size_t i = 0; i < num_to_grab; i++) {
      out_buffer[i] = tasks_.PopFront();
      size_--;
    }

    return num_to_grab;
  }

  // compatibility with lockfree impl below
  moodycamel::ConsumerToken GetConsumerToken() {
    return moodycamel::ConsumerToken{queue_};
  }

  moodycamel::ProducerToken GetProducerToken() {
    return moodycamel::ProducerToken{queue_};
  }

 private:
  wheels::IntrusiveList<Task> tasks_{};
  threads::lockfull::SpinLock mutex_;
  size_t size_{0};

  // compatibility with lockfree impl
  moodycamel::ConcurrentQueue<std::monostate> queue_{0};
};

// Concurrent queue is not sequentially consistent 
// and we must enforce this consistency via thread_fence
class GlobalQueueLockfreeImpl {
 public:
  moodycamel::ConsumerToken GetConsumerToken() {
    return moodycamel::ConsumerToken{queue_};
  }

  moodycamel::ProducerToken GetProducerToken() {
    return moodycamel::ProducerToken{queue_};
  }

  void Push(Task* task) {
    queue_.enqueue_bulk(&task, 1);
  }

  // Called only by workers with tokens

  void Push(Task* task, moodycamel::ProducerToken& token) {
    queue_.enqueue_bulk(token, &task, 1);
  }

  void Append(std::span<Task*> tasks, moodycamel::ProducerToken& token) {
    queue_.enqueue_bulk(token, tasks.begin(), tasks.size());
  }

  // Returns nullptr if queue is empty
  Task* TryPop(moodycamel::ConsumerToken& token) {
    Task* buffer = nullptr;

    queue_.try_dequeue_bulk(token, &buffer, 1);

    return buffer;
  }

  // Returns number of items in `out_buffer`
  size_t Grab(std::span<Task*> out_buffer, size_t workers,
              moodycamel::ConsumerToken& token) {
    size_t size = std::min(queue_.size_approx() / workers, out_buffer.size());

    if (size == 0) {
      size = 1;
    }

    size_t ret = queue_.try_dequeue_bulk(token, out_buffer.begin(), size);

    return ret;
  }

 private:
  moodycamel::ConcurrentQueue<Task*> queue_;
};

}  // namespace weave::executors::tp::fast
