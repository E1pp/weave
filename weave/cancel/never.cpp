#include <weave/cancel/never.hpp>

#include <cstdlib>

namespace weave::cancel {

struct NeverState final : public SignalSender {
  bool Cancellable() override final {
    return false;
  }

  bool CancelRequested() override final {
    return false;
  }

  void Attach(SignalReceiver* receiver) override final {
    receiver->Forward(Signal::Release());
  }

  void Detach(SignalReceiver*) override final {
    // No-Op
  }

  ~NeverState() override final = default;
};

Token Never() {
  static NeverState instance;

  return Token::Fabricate(&instance);
}

}  // namespace weave::cancel