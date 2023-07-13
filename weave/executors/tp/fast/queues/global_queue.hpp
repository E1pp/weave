#pragma once

#include <weave/executors/task.hpp>

#include <weave/threads/blocking/stdlike/mutex.hpp>
#include <weave/threads/blocking/spinlock.hpp>

#include <twist/ed/stdlike/mutex.hpp>

#include <wheels/intrusive/list.hpp>

#include <span>

namespace weave::executors::tp::fast {

class GlobalQueueBlockingImpl;

class GlobalQueueLockfreeImpl; // ConcurrentQueue is not seq_cst 
                               // also cringe and fake + L + ratio

// Unbounded queue shared between workers
using GlobalQueue = GlobalQueueBlockingImpl;

// Check different mutexes
class GlobalQueueBlockingImpl {
 public:
  void Push(Task* item) {
    threads::blocking::stdlike::LockGuard lock(mutex_);

    tasks_.PushBack(item);
    size_++;
  }

  void Append(std::span<Task*> overflow) {
    // prepare a list to merge
    wheels::IntrusiveList<Task> overflow_l;

    for (auto node : overflow) {
      overflow_l.PushBack(node);
    }

    threads::blocking::stdlike::LockGuard lock(mutex_);

    tasks_.Append(overflow_l);
    size_ += overflow.size();
  }

  // Returns nullptr if queue is empty
  Task* TryPop() {
    threads::blocking::stdlike::LockGuard lock(mutex_);

    Task* task;
    if ((task = tasks_.PopFront()) != nullptr) {
      size_--;
    }

    return task;
  }

  // Returns number of items in `out_buffer`
  size_t Grab(std::span<Task*> out_buffer, size_t workers) {
    threads::blocking::stdlike::LockGuard lock(mutex_);

    size_t num_to_grab = std::min(
        size_ < workers && size_ != 0 ? 1 : size_ / workers, out_buffer.size());

    for (size_t i = 0; i < num_to_grab; i++) {
      out_buffer[i] = tasks_.PopFront();
      size_--;
    }

    return num_to_grab;
  }

 private:
  wheels::IntrusiveList<Task> tasks_{};
  threads::blocking::SpinLock mutex_;
  size_t size_{0};
};

}  // namespace weave::executors::tp::fast
