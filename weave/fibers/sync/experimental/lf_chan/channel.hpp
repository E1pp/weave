#pragma once

#include <weave/fibers/sync/experimental/lf_chan/detail/segment.hpp>

#include <weave/threads/lockfree/hazard/manager.hpp>

#include <cstdlib>
#include <deque>
#include <memory>
#include <optional>

namespace weave::fibers::experimental {

//////////////////////////////////////////////////////////////////////

template <typename T>
class Channel;

namespace detail {

template <typename T>
class ChannelImpl {
  using Segment = Segment<T>;

  template <typename U>
  friend class weave::fibers::experimental::Channel;

 public:
  explicit ChannelImpl() {
    SetupQueue();
  }

  void Send(T value) {
    Waiter<T> waiter(State::Sender);
    waiter.WriteValue(value);

    auto awaiter = [&](FiberHandle handle) {
      auto mutator = gc_->MakeMutator(2);

      waiter.SetFiber(handle);

      while (true) {
        size_t enq_idx = enqueue_index_.load();
        size_t deq_idx = dequeue_index_.load();

        if (enq_idx < deq_idx) {
          // inconsistent data -> try again
          continue;
        }

        if (deq_idx == enq_idx) {
          // empty queue observed -> try to park
          if (TryEnqueue(enq_idx, &waiter)) {
            // we parked
            return FiberHandle::Invalid();
          } else {
            // concurrent thread updated queue -> try again
            continue;
          }
        }

        // Segment* curr_head = head_.load();
        Segment* curr_head = mutator.Protect(0, head_);

        if (deq_idx / kSegmentSize < curr_head->id_) {
          // state is inconsistent -> try again
          continue;
        }

        if (deq_idx / kSegmentSize > curr_head->id_) {
          // head is outdated -> helping and try again

          if (head_.compare_exchange_weak(curr_head, curr_head->next_.load())) {
            // curr_head at this point is empty and can be safely retired
            mutator.Retire(curr_head);
          }

          continue;
        }

        // read element from the segment
        size_t idx_in_seg = deq_idx % kSegmentSize;
        State top_status = ReadStatusFromSegment(curr_head, idx_in_seg);

        if (top_status == State::Null) {
          // we got state which was not ready
          // -> try again to either clear it
          // or find another one
          continue;
        }

        if (top_status == State::Broken) {
          // slot is broken -> move idx and try again
          dequeue_index_.compare_exchange_weak(deq_idx, deq_idx + 1);
          continue;
        }

        if (top_status == State::Receiver) {
          // try to actually dequeue and rendezvous with receiver
          if (TryRendezvousWithReceiver(curr_head, deq_idx, &waiter)) {
            // we got our receiver -> resume this fiber
            return handle;
          }
          // concurrent thread has updated state -> try again
        } else {
          // we've observed sender instead -> try park
          if (TryEnqueue(enq_idx, &waiter)) {
            return FiberHandle::Invalid();
          }
          // concurrent thread has updated state -> try again
        }
      }
    };

    Suspend(awaiter);
  }

  bool TrySend(T value) {
    Waiter<T> waiter(State::Sender);
    waiter.WriteValue(value);

    auto mutator = gc_->MakeMutator(2);

    while (true) {
      size_t enq_idx = enqueue_index_.load();
      size_t deq_idx = dequeue_index_.load();

      if (enq_idx < deq_idx) {
        // inconsistent data -> try again
        continue;
      }

      if (deq_idx == enq_idx) {
        // empty queue observed -> fail!
        return false;
      }

      // Segment* curr_head = head_.load();
      Segment* curr_head = mutator.Protect(0, head_);

      if (deq_idx / kSegmentSize < curr_head->id_) {
        // state is inconsistent -> try again
        continue;
      }

      if (deq_idx / kSegmentSize > curr_head->id_) {
        // head is outdated -> helping and try again

        if (head_.compare_exchange_weak(curr_head, curr_head->next_.load())) {
          // curr_head at this point is empty and can be safely retired
          mutator.Retire(curr_head);
        }

        continue;
      }

      // read element from the segment
      size_t idx_in_seg = deq_idx % kSegmentSize;
      State top_status = ReadStatusFromSegment(curr_head, idx_in_seg);

      if (top_status == State::Null) {
        // we got state which was not ready
        // -> try again to either clear it
        // or find another one
        continue;
      }

      if (top_status == State::Broken) {
        // slot is broken -> move idx and try again
        dequeue_index_.compare_exchange_weak(deq_idx, deq_idx + 1);
        continue;
      }

      if (top_status == State::Receiver) {
        // try to actually dequeue and rendezvous with receiver
        if (TryRendezvousWithReceiver(curr_head, deq_idx, &waiter)) {
          // we got our receiver -> success!
          return true;
        }
        // concurrent thread has updated state -> try again
      } else {
        // we've observed sender instead -> fail!
        return false;
      }
    }
  }

  T Receive() {
    Waiter<T> waiter(State::Receiver);

    auto awaiter = [&](FiberHandle handle) {
      auto mutator = gc_->MakeMutator(2);
      waiter.SetFiber(handle);

      while (true) {
        size_t enq_idx = enqueue_index_.load();
        size_t deq_idx = dequeue_index_.load();

        if (enq_idx < deq_idx) {
          // inconsistent data -> try again
          continue;
        }

        if (deq_idx == enq_idx) {
          // empty queue observed -> try to park

          if (TryEnqueue(enq_idx, &waiter)) {
            // we parked

            return FiberHandle::Invalid();
          } else {
            // concurrent thread updated queue -> try again
            continue;
          }
        }

        // Segment* curr_head = head_.load();
        Segment* curr_head = mutator.Protect(0, head_);

        if (deq_idx / kSegmentSize < curr_head->id_) {
          // state is inconsistent -> try again
          continue;
        }

        if (deq_idx / kSegmentSize > curr_head->id_) {
          // head is outdated -> helping and try again

          if (head_.compare_exchange_weak(curr_head, curr_head->next_.load())) {
            // curr_head can be safely retired since it is empty
            mutator.Retire(curr_head);
          }

          continue;
        }

        // read element from the segment
        size_t idx_in_seg = deq_idx % kSegmentSize;
        State top_status = ReadStatusFromSegment(curr_head, idx_in_seg);

        if (top_status == State::Null) {
          // we got state which was not ready
          // -> try again to either clear it
          // or find another one
          continue;
        }

        if (top_status == State::Broken) {
          // slot is broken -> move idx and try again
          dequeue_index_.compare_exchange_weak(deq_idx, deq_idx + 1);
          continue;
        }

        if (top_status == State::Sender) {
          // try to actually dequeue and rendezvous with sender
          if (TryRendezvousWithSender(curr_head, deq_idx, &waiter)) {
            // we got our sender -> resume this fiber
            return handle;
          }
          // concurrent thread has updated state -> try again
        } else {
          // we've observed receiver instead -> try park
          if (TryEnqueue(enq_idx, &waiter)) {
            return FiberHandle::Invalid();
          }
          // concurrent thread has updated state -> try again
        }
      }
    };

    Suspend(awaiter);

    // waiter has value
    return waiter.ReadValue();
  }

  std::optional<T> TryReceive() {
    Waiter<T> waiter(State::Receiver);
    auto mutator = gc_->MakeMutator(2);

    while (true) {
      size_t enq_idx = enqueue_index_.load();
      size_t deq_idx = dequeue_index_.load();

      if (enq_idx < deq_idx) {
        // inconsistent data -> try again
        continue;
      }

      if (deq_idx == enq_idx) {
        // empty queue observed -> fail!
        return std::nullopt;
      }

      // Segment* curr_head = head_.load();
      Segment* curr_head = mutator.Protect(0, head_);

      if (deq_idx / kSegmentSize < curr_head->id_) {
        // state is inconsistent -> try again
        continue;
      }

      if (deq_idx / kSegmentSize > curr_head->id_) {
        // head is outdated -> helping and try again

        if (head_.compare_exchange_weak(curr_head, curr_head->next_.load())) {
          // curr_head can be safely retired since it is empty
          mutator.Retire(curr_head);
        }

        continue;
      }

      // read element from the segment
      size_t idx_in_seg = deq_idx % kSegmentSize;
      State top_status = ReadStatusFromSegment(curr_head, idx_in_seg);

      if (top_status == State::Null) {
        // we got state which was not ready
        // -> try again to either clear it
        // or find another one
        continue;
      }

      if (top_status == State::Broken) {
        // slot is broken -> move idx and try again
        dequeue_index_.compare_exchange_weak(deq_idx, deq_idx + 1);
        continue;
      }

      if (top_status == State::Sender) {
        // try to actually dequeue and rendezvous with sender
        if (TryRendezvousWithSender(curr_head, deq_idx, &waiter)) {
          // we got our sender -> success!
          break;
        }
        // concurrent thread has updated state -> try again
      } else {
        // we've observed receiver instead -> fail!
        return std::nullopt;
      }
    }

    // waiter has value
    return waiter.ReadValue();
  }

  ~ChannelImpl() {
    Segment* head = head_.load();

    while (head != nullptr) {
      Segment* tmp = head;
      head = head->next_.load();
      delete tmp;
    }
  }

 private:
  void SetupQueue() {
    Segment* dummy = new Segment(0);
    head_.store(dummy);
    tail_.store(dummy);
  }

  bool TryEnqueue(size_t idx, IWaiter<T>* waiter) {
    auto mutator = gc_->MakeMutator(2);

    // Segment* curr_tail = tail_.load();
    Segment* curr_tail = mutator.Protect(1, tail_);

    if (curr_tail->id_ > idx / kSegmentSize) {
      // tail is inconsistent -> try again
      return false;
    }

    if (curr_tail->id_ == idx / kSegmentSize && (idx % kSegmentSize == 0)) {
      // another thread with the same idx
      // pushed a new tail already
      // we help them with advancing the index
      // and then retry enqueue
      enqueue_index_.compare_exchange_weak(idx, idx + 1);
      return false;
    }

    if (idx % kSegmentSize != 0) {
      return TryEnqueueInSegment(curr_tail, idx, waiter);
    } else {
      return TryEnqueueAlloc(curr_tail, idx, waiter);
    }
  }

  bool TryEnqueueAlloc(Segment* tail, size_t idx, IWaiter<T>* waiter) {
    Segment* new_tail = new Segment(tail->id_ + 1);

    new_tail->array_[0].waiter_.store(waiter);
    new_tail->array_[0].state_.store(waiter->GetStatus());

    while (true) {
      if (tail->next_.load() != nullptr) {
        // help another thread
        tail_.compare_exchange_strong(tail, tail->next_.load());
        enqueue_index_.compare_exchange_weak(idx, idx + 1);

        delete new_tail;
        return false;
      }

      Segment* null_ptr = nullptr;
      if (tail->next_.compare_exchange_weak(null_ptr, new_tail)) {
        // others could have helped us

        tail_.compare_exchange_strong(tail, new_tail);
        enqueue_index_.compare_exchange_weak(idx, idx + 1);

        return true;
      }
    }
  }

  bool TryEnqueueInSegment(Segment* tail, size_t idx, IWaiter<T>* waiter) {
    if (!enqueue_index_.compare_exchange_weak(idx, idx + 1)) {
      // failed to advance the index -> try again
      return false;
    }

    size_t seg_idx = idx % kSegmentSize;

    tail->array_[seg_idx].waiter_.store(waiter);

    State nu_ll = State::Null;
    return tail->array_[seg_idx].state_.compare_exchange_weak(
        nu_ll, waiter->GetStatus());
    // true : changes were committed correctly
    // false : concurrent ReadStatusFromSegment
    // has observed Null and replaced it with
    // Broken -> try again
  }

  State ReadStatusFromSegment(Segment* head, size_t idx) {
    State status = head->array_[idx].state_.load();

    if (status != State::Null) {
      return status;
    }

    // status == Null
    head->array_[idx].state_.compare_exchange_strong(status, State::Broken);
    return status;
    // status == Null : we replaced with Broken
    // status != Null : state_ was no longer Null
    // we got fresh value

    // since reading Null can result in suspension
    // we want to make sure that no spur fails occur
  }

  bool TryRendezvousWithReceiver(Segment* head, size_t idx,
                                 IWaiter<T>* sender) {
    if (!dequeue_index_.compare_exchange_weak(idx, idx + 1)) {
      // someone has taken this pointer already
      return false;
    }

    size_t seg_idx = idx % kSegmentSize;

    IWaiter<T>* receiver = head->array_[seg_idx].waiter_.load();
    head->array_[seg_idx].state_.store(State::Null);

    T obj = sender->ReadValue();
    if (receiver->WriteValue(obj)) {
      // we can resume receiver now
      receiver->Schedule();
      return true;
    }

    std::abort();

    return false;
  }

  bool TryRendezvousWithSender(Segment* head, size_t idx,
                               IWaiter<T>* receiver) {
    if (!dequeue_index_.compare_exchange_weak(idx, idx + 1)) {
      // someone has taken this pointer already
      return false;
    }

    size_t seg_idx = idx % kSegmentSize;

    IWaiter<T>* sender = head->array_[seg_idx].waiter_.load();
    head->array_[seg_idx].state_.store(State::Null);

    T result = sender->ReadValue();

    if (receiver->WriteValue(result)) {
      sender->Schedule();
      return true;
    }

    std::abort();

    return false;
  }

 private:
  twist::ed::stdlike::atomic<Segment*> head_{nullptr};
  twist::ed::stdlike::atomic<Segment*> tail_{nullptr};

  twist::ed::stdlike::atomic<size_t> enqueue_index_{1};
  twist::ed::stdlike::atomic<size_t> dequeue_index_{1};

  threads::hazard::Manager* gc_{threads::hazard::Manager::Get()};
};

}  // namespace detail

//////////////////////////////////////////////////////////////////////

// Non-buffered Multi-Producer / Multi-Consumer Lockfree Channel
// https://tour.golang.org/concurrency/3

// Does not support void type
// Use weave::Unit from weave/result/types/unit.hpp

template <typename T>
class Channel {
  using Impl = detail::ChannelImpl<T>;

 public:
  explicit Channel()
      : impl_(std::make_shared<Impl>()) {
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

}  // namespace weave::fibers::experimental
