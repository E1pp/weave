#pragma once

#include <atomic>
#include <weave/cancel/token.hpp>

#include <weave/threads/lockfree/ref_count.hpp>

#include <weave/support/word.hpp>

namespace weave::cancel::sources {

template <bool OnHeap>
class JoinSource
    : public SignalSender,
      public SignalReceiver,
      public threads::lockfree::RefCounter<JoinSource<OnHeap>, OnHeap> {
  using State = support::Word;

  static constexpr uint32_t kInit = 0;
  static constexpr uint32_t kCancelled = 1;

 public:
  using Base = threads::lockfree::RefCounter<JoinSource<OnHeap>, OnHeap>;

  explicit JoinSource() noexcept
      : state_(State::Value(kInit)) {
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
    return IsCancelled(state_.load(std::memory_order::acquire));
  }

  bool Cancellable() override {
    return true;
  }

  void Attach(SignalReceiver* receiver) override {
    State curr = state_.load(std::memory_order::acquire);

    do {
      if (IsCancelled(curr)) {
        receiver->Forward(Signal::Cancel());
        return;
      }

      if (curr.IsPointer()) {
        // Has another receiver in queue
        receiver->next_ = curr.AsPointerTo<SignalReceiver>();
      } else {
        receiver->next_ = nullptr;
      }

    } while (!state_.compare_exchange_weak(curr, State::Pointer(receiver),
                                           std::memory_order::release,
                                           std::memory_order::acquire));
  }

  void Detach(SignalReceiver*) override {
    // Not Supported :*(
  }

  void RequestCancel() {
    State prev =
        state_.exchange(State::Value(kCancelled), std::memory_order::acq_rel);

    if (prev.IsPointer()) {
      SignalReceiver* receiver_head = prev.AsPointerTo<SignalReceiver>();

      [](SignalReceiver* receiver, Signal signal) {
        while (receiver != nullptr) {
          auto* next = static_cast<SignalReceiver*>(receiver->Next());

          receiver->Forward(signal);

          receiver = next;
        }
      }(receiver_head, Signal::Cancel());
    }
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
  // Helps with interpetation
  static bool IsCancelled(State state) {
    return state.IsValue() && state.AsValue() == kCancelled;
  }

  static bool IsInit(State state) {
    return state.IsValue() && state.AsValue() == kInit;
  }

 private:
  twist::ed::stdlike::atomic<State> state_;
};

}  // namespace weave::cancel::sources