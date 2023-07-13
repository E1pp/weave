#pragma once

#include <weave/threads/blocking/stdlike/condition_variable.hpp>
#include <weave/threads/blocking/stdlike/mutex.hpp>

#include <wheels/intrusive/list.hpp>

#include <mutex>
#include <optional>

namespace weave::threads::blocking {

// Unbounded blocking multi-producers/multi-consumers (MPMC) queue

template <typename T>
class UnboundedBlockingQueue {
 public:
  // returns true if object was put
  // returns false if queue was closed
  bool Put(T* object) {
    std::lock_guard lock(mutex_);

    if (is_open_) {
      queue_.PushBack(object);
      queue_is_not_empty_.NotifyOne();
    }

    return is_open_;
  }

  // returns nullptr iff the queue is empty
  T* Take() {
    std::unique_lock lock(mutex_);
    T* return_value = nullptr;

    while (is_open_ && queue_.IsEmpty()) {
      queue_is_not_empty_.Wait(lock);
    }

    return_value = queue_.PopFront();

    return return_value;
  }

  void Close() {
    std::lock_guard lock(mutex_);
    is_open_ = false;
    queue_is_not_empty_.NotifyAll();
  }

 private:
  bool is_open_{true};
  wheels::IntrusiveList<T> queue_;  // guarded by mutex_
  threads::blocking::stdlike::Mutex mutex_;
  threads::blocking::stdlike::CondVar queue_is_not_empty_;
};

}  // namespace weave::threads::blocking