#pragma once

#include <weave/cancel/token.hpp>

#include <weave/support/word.hpp>

#include <twist/ed/stdlike/atomic.hpp>

namespace weave::cancel::sources {

// Source to pass signal through
// 1P1C heap allocated rendezvous point
class StrandSource : public SignalSender,
                     public SignalReceiver {
  using State = support::Word;

  static constexpr uint32_t kInit = 0;
  static constexpr uint32_t kCancelled = 1;

 public:
  StrandSource()
      : state_(State::Value(kInit)) {
  }

  ~StrandSource() override = default;

  // SignalSender
  bool CancelRequested() override;

  bool Cancellable() override {
    return true;
  }

  void Attach(SignalReceiver* receiver) override {
    SetReceiver(receiver);
  }

  void Detach(SignalReceiver*) override {
    ClearReceiver();
  }

  // SignalReceiver
  void RequestCancel();

  void Forward(Signal) override;

 protected:
  void AddRef();

  void ReleaseRef();

 private:
  // Helps with interpetation
  static bool IsCancelled(State state) {
    return state.IsValue() && state.AsValue() == kCancelled;
  }

  static bool IsInit(State state) {
    return state.IsValue() && state.AsValue() == kInit;
  }

  void SetReceiver(SignalReceiver*);

  void ClearReceiver();

 private:
  twist::ed::stdlike::atomic<State> state_;
  twist::ed::stdlike::atomic<size_t> ref_count_{0};
};

}  // namespace weave::cancel::sources