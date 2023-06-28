#pragma once

#include <weave/fibers/sched/suspend.hpp>

#include <weave/fibers/sync/detail/meta.hpp>

#include <weave/fibers/sync/waiters.hpp>

#include <weave/threads/lockfull/stdlike/mutex.hpp>
#include <weave/threads/lockfull/spinlock.hpp>

#include <weave/support/cyclic_buffer.hpp>

#include <wheels/intrusive/list.hpp>

#include <cstdlib>
#include <deque>
#include <memory>
#include <optional>

namespace weave::fibers {

//////////////////////////////////////////////////////////////////////

template <typename T>
class Channel;

namespace detail {

template <bool TryVersion, SelectorAlternative... Types>
class Selector;

template <bool TryVersion, size_t Index, SelectorAlternative U,
          SelectorAlternative... Types>
struct SelectorLeaf;

template <typename T>
class ChannelWaiter final : public ICargoWaiter<T> {
 public:
  void SetHandle(FiberHandle handle) override final {
    handle_ = handle;
  }

  void Schedule(executors::SchedulerHint) override final {
    handle_.Schedule(executors::SchedulerHint::Next);
  }

  void WriteValue(T val) override final {
    storage_.emplace(std::move(val));
  }

  T ReadValue() override final {
    return std::move(*storage_);
  }

  State MarkUsed() override final {
    // Uncancellable
    return State::Ready;
  }

  ~ChannelWaiter() override final = default;

 private:
  FiberHandle handle_{FiberHandle::Invalid()};

  std::optional<T> storage_{};
};

enum RendezvousResult : int { Success = 0, Fail = 1, Cancelled = 2 };

template <typename T>
class ChannelImpl {
  template <typename U>
  friend class weave::fibers::Channel;

  template <bool TryVersion, SelectorAlternative... Types>
  friend class Selector;

 public:
  explicit ChannelImpl(size_t capacity)
      : capacity_(capacity),
        storage_(capacity) {
  }

  void Send(T value) {
    ChannelWaiter<T> sender;
    sender.WriteValue(std::move(value));

    threads::lockfull::stdlike::UniqueLock lock(chan_spinlock_);

    if (TryCompleteSender(&sender) == RendezvousResult::Success) {
      return;
    }

    auto send_awaiter = [&](FiberHandle handle) mutable {
      sender.SetHandle(handle);

      queue_.PushBack(&sender);
      lock.Unlock();

      return FiberHandle::Invalid();
    };

    Suspend(send_awaiter);
  }

  bool TrySend(T value) {
    ChannelWaiter<T> sender;
    sender.WriteValue(std::move(value));

    threads::lockfull::stdlike::LockGuard lock(chan_spinlock_);
    return TryCompleteSender(&sender) == RendezvousResult::Success;
  }

  T Receive() {
    ChannelWaiter<T> receiver;

    threads::lockfull::stdlike::UniqueLock lock(chan_spinlock_);

    if (TryCompleteReceiver(&receiver) != RendezvousResult::Success) {
      auto receive_awaiter = [&](FiberHandle handle) mutable {
        receiver.SetHandle(handle);

        queue_.PushBack(&receiver);
        lock.Unlock();

        return FiberHandle::Invalid();
      };

      Suspend(receive_awaiter);
    }

    return receiver.ReadValue();
  }

  std::optional<T> TryReceive() {
    ChannelWaiter<T> receiver;

    threads::lockfull::stdlike::LockGuard lock(chan_spinlock_);
    if (TryCompleteReceiver(&receiver) == RendezvousResult::Success) {
      return receiver.ReadValue();
    }

    return std::nullopt;
  }

 private:
  // Under spinlock
  RendezvousResult TryCompleteSender(ICargoWaiter<T>* sender) {
    if (storage_.IsFull()) {
      // if storage is full then queue is either empty
      // (we are the first sender to observe full storage)
      // or contains senders
      // (who observed full storage right before us)
      return RendezvousResult::Fail;
    }

    if (sender->MarkUsed() == State::Used) {
      // sender was completed by someone else
      // therefore contains no value
      return RendezvousResult::Cancelled;
    }

    while (ICargoWaiter<T>* next_receiver = queue_.PopFront()) {
      if (next_receiver->MarkUsed() != State::Used) {
        next_receiver->WriteValue(sender->ReadValue());
        next_receiver->Schedule();
        return RendezvousResult::Success;
      }
    }

    storage_.TryPush(sender->ReadValue());
    return RendezvousResult::Success;
  }

  // Under spinlock
  RendezvousResult TryCompleteReceiver(ICargoWaiter<T>* receiver) {
    if (storage_.IsEmpty()) {
      // if storage is empty queue_ is either empty
      // (we are first to observe empty storage)
      // or queue_ has a receiver
      // (who observed empty storage right before us)
      return RendezvousResult::Fail;
    }

    if (receiver->MarkUsed() == State::Used) {
      // receiver was fulfilled by someone else
      // therefore already contains value
      return RendezvousResult::Cancelled;
    }

    T value = std::move(*storage_.TryPop());
    receiver->WriteValue(std::move(value));

    // now we need to check if we can wake someone in queue
    while (ICargoWaiter<T>* next_sender = queue_.PopFront()) {
      // next_in_queue role is Sender
      if (next_sender->MarkUsed() != State::Used) {
        storage_.TryPush(next_sender->ReadValue());
        next_sender->Schedule();
        break;
      }
    }

    return RendezvousResult::Success;
  }

 private:
  const size_t capacity_;
  threads::lockfull::SpinLock chan_spinlock_;  // Guards storage_

  support::CyclicBuffer<T> storage_;
  wheels::IntrusiveList<ICargoWaiter<T>> queue_{};
};

}  // namespace detail

//////////////////////////////////////////////////////////////////////

// Buffered Multi-Producer / Multi-Consumer Channel
// https://tour.golang.org/concurrency/3

// Does not support void type
// Use weave::Unit instead

template <typename T>
class Channel {
  using Impl = detail::ChannelImpl<T>;

  template <bool TryVersion, SelectorAlternative... Types>
  friend class detail::Selector;

 public:
  // Bounded channel, `capacity` > 0
  explicit Channel(size_t capacity)
      : impl_(std::make_shared<Impl>(capacity)) {
  }

  // Suspending
  void Send(T value) {
    impl_->Send(std::move(value));
  }

  bool TrySend(T value) {
    return impl_->TrySend(std::move(value));
  }

  // Suspending
  T Receive() {
    return impl_->Receive();
  }

  std::optional<T> TryReceive() {
    return impl_->TryReceive();
  }

 private:
  std::shared_ptr<Impl> impl_;
};

}  // namespace weave::fibers
