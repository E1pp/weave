#pragma once

#include <weave/threads/blocking/stdlike/mutex.hpp>
#include <weave/threads/blocking/spinlock.hpp>

#include <wheels/intrusive/list.hpp>

namespace weave::executors::tp::fast {

template <typename T>
class ParkingLotBlockingImpl;

template <typename T>
class ParkingLotLockfreeImpl;

template <typename T>
using ParkingLot = ParkingLotBlockingImpl<T>;

template <typename T>
class ParkingLotBlockingImpl {
 public:
  using Node = wheels::IntrusiveListNode<T>;
  // post condition: obj is on parking lot
  void Enqueue(Node* obj) {
    threads::blocking::stdlike::LockGuard lock(spinlock_);
    if (obj->IsLinked()) {
      return;
    }
    lot_.PushBack(obj);
  }

  T* TryDequeue() {
    threads::blocking::stdlike::LockGuard lock(spinlock_);
    return lot_.PopFront();
  }

  // post condition: key is not present on parking lot
  void RemoveFromQueue(Node* obj) {
    threads::blocking::stdlike::LockGuard lock(spinlock_);
    obj->Unlink();
  }

  ~ParkingLotBlockingImpl() {
    lot_.UnlinkAll();
  }

 private:
  threads::blocking::SpinLock spinlock_;  // guards the container
  wheels::IntrusiveList<T> lot_;
};

}  // namespace weave::executors::tp::fast