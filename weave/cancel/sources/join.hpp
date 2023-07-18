#pragma once

#include <weave/cancel/token.hpp>

#include <weave/support/delayed.hpp>

#include <weave/threads/lockfree/ref_count.hpp>
#include <weave/threads/lockfree/tagged_buffer.hpp>

namespace weave::cancel::sources {

template <bool OnHeap>
class JoinSource
    : public SignalSender,
      public SignalReceiver,
      public threads::lockfree::RefCounter<JoinSource<OnHeap>, OnHeap> {
 public:
  using Base = threads::lockfree::RefCounter<JoinSource<OnHeap>, OnHeap>;

  explicit JoinSource(size_t capacity) noexcept
      : buffer_(capacity) {
  }

  // Non-copyable
  JoinSource(const JoinSource&) = delete;
  JoinSource& operator=(const JoinSource&) = delete;

  // Non-movable
  JoinSource(JoinSource&&) = delete;
  JoinSource& operator=(JoinSource&&) = delete;

  ~JoinSource() override = default;

  // SignalSender
  bool CancelRequested() override {
    return buffer_.IsSealed();
  }

  bool Cancellable() override {
    return true;
  }

  void Attach(SignalReceiver* receiver) override {
    if (!buffer_.TryPush(receiver)) {
      receiver->Forward(Signal::Cancel());
    }
  }

  void Detach(SignalReceiver* receiver) override {
    buffer_.Pop(receiver);
    receiver->Forward(Signal::Release());
  }

  void RequestCancel() {
    auto processor = +[](SignalReceiver* receiver) {
      receiver->Forward(Signal::Cancel());
    };

    buffer_.SealBuffer(processor);
  }

  // SignalReceiver
  void Forward(Signal signal) override {
    if (signal.CancelRequested()) {
      RequestCancel();
    } else {
      // We still will want
      // to cancel some producers
    }

    Base::ReleaseRef();
  }

 private:
  threads::lockfree::TaggedBuffer<SignalReceiver> buffer_;
};

}  // namespace weave::cancel::sources