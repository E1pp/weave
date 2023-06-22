#pragma once

#include <weave/threads/lockfull/stdlike/mutex.hpp>
#include <weave/threads/lockfull/spinlock.hpp>
#include <twist/ed/stdlike/mutex.hpp>

#include <wheels/intrusive/list.hpp>

#include <concurrentqueue.h>

namespace weave::executors::tp::fast {

template <typename T>
class ParkingLotBlockingImpl;

template <typename T>
class ParkingLotLockfreeImpl;

template <typename T>
using ParkingLot = ParkingLotLockfreeImpl<T>;

template <typename T>
class ParkingLotBlockingImpl {
 public:
  using Node = wheels::IntrusiveListNode<T>;
  // post condition: obj is on parking lot
  void Enqueue(Node* obj) {
    threads::lockfull::stdlike::LockGuard lock(spinlock_);
    if (obj->IsLinked()) {
      return;
    }
    lot_.PushBack(obj);
  }

  T* TryDequeue() {
    threads::lockfull::stdlike::LockGuard lock(spinlock_);
    return lot_.PopFront();
  }

  // post condition: key is not present on parking lot
  void RemoveFromQueue(Node* obj) {
    threads::lockfull::stdlike::LockGuard lock(spinlock_);
    obj->Unlink();
  }

  ~ParkingLotBlockingImpl() {
    lot_.UnlinkAll();
  }

 private:
  threads::lockfull::SpinLock spinlock_;  // guards the container
  // twist::ed::stdlike::mutex spinlock_;
  wheels::IntrusiveList<T> lot_;
};

// we don't need producers/consumers to
// be properly connected so we just
// generate tokens on stack just to
// ensure explicit overloads are used
// which seem to not race with each other
template <typename T>
class ParkingLotLockfreeImpl {
 public:
  void Enqueue(T* obj) {
    moodycamel::ProducerToken token{lot_};
    lot_.enqueue_bulk(token, &obj, 1);
  }

  T* TryDequeue() {
    T* ptr = nullptr;
    moodycamel::ConsumerToken token{lot_};

    return lot_.try_dequeue_bulk(token, &ptr, 1) ? ptr : nullptr;
  }

 private:
  moodycamel::ConcurrentQueue<T*> lot_;
};

}  // namespace weave::executors::tp::fast