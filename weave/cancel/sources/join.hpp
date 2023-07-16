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

  // Movable
  JoinSource(JoinSource&&) = default;
  JoinSource& operator=(JoinSource&&) = default;

  ~JoinSource() override {
    buffer_.Destroy();
  }

  // SignalSender
  bool CancelRequested() override {
    return buffer_.Refer().IsSealed();
  }

  bool Cancellable() override {
    return true;
  }

  void Attach(SignalReceiver* receiver) override {
    if (!buffer_.Refer().TryPush(receiver)) {
      receiver->Forward(Signal::Cancel());
    }
  }

  void Detach(SignalReceiver* receiver) override {
    buffer_.Refer().Pop(receiver);
    receiver->Forward(Signal::Release());
  }

  void RequestCancel() {
    auto processor = +[](SignalReceiver* receiver) {
      receiver->Forward(Signal::Cancel());
    };

    buffer_.Refer().SealBuffer(processor);
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

 protected:
  void Create(size_t capacity) {
    buffer_.Create(capacity);
  }

 private:
  support::MaybeDelayed<!OnHeap,
                        threads::lockfree::TaggedBuffer<SignalReceiver>>
      buffer_;
};

}  // namespace weave::cancel::sources