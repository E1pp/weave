#include <weave/cancel/sources/strand.hpp>

namespace weave::cancel::sources {

//////////////////////////////////////////////////////////////

// Helper methods

void StrandSource::SetReceiver(SignalReceiver* receiver) {
  State curr = State::Value(kInit);

  if (state_.compare_exchange_strong(curr, State::Pointer(receiver),
                                     std::memory_order::release,
                                     std::memory_order::relaxed)) {
    return;  // Attached
  }

  WHEELS_VERIFY(!curr.IsPointer(), "Broken state with > 1 receivers");

  WHEELS_VERIFY(curr.AsValue() == kCancelled, "Wrong state!");

  // Cancel receiver
  receiver->Forward(Signal::Cancel());
}

void StrandSource::ClearReceiver(SignalReceiver* expected) {
  State curr = state_.load(std::memory_order::acquire);

  if (IsCancelled(curr) || IsInit(curr)) {
    return;
  }

  auto* receiver = curr.AsPointerTo<SignalReceiver>();

  if (expected != kAnyOne) {
    WHEELS_VERIFY(expected == receiver, "Expected a different receiver!");
  }

  #if (NDEBUG)
  #define MEMORY_ORDER_ON_SUCCESS std::memory_order::release
  #else
  #define MEMORY_ORDER_ON_SUCCESS std::memory_order::acq_rel
  #endif

  bool ok = state_.compare_exchange_strong(curr, State::Value(kInit),
                                           MEMORY_ORDER_ON_SUCCESS,
                                           std::memory_order::relaxed);



  if (ok) {
    receiver->Forward(Signal::Release());
    return;
  } 
  
  #if !(NDEBUG)
  if (expected != kAnyOne) {
    WHEELS_VERIFY(!curr.IsPointer(), "Broken state: different receiver!");

    if (IsInit(curr) || IsCancelled(curr)) {
      return;
    }

    WHEELS_PANIC("Unreachable");
  }
  #endif
}

//////////////////////////////////////////////////////////////

bool StrandSource::CancelRequested() {
  return IsCancelled(state_.load(std::memory_order::acquire));
}

void StrandSource::RequestCancel() {
  State prev =
      state_.exchange(State::Value(kCancelled), std::memory_order::acq_rel);

  if (prev.IsPointer()) {
    SignalReceiver* receiver = prev.AsPointerTo<SignalReceiver>();

    receiver->Forward(Signal::Cancel());
  }
}

void StrandSource::Forward(Signal signal) {
  if (signal.CancelRequested()) {
    RequestCancel();
  } else {
    ClearReceiver(kAnyOne);
  }

  // Memory Management
  Base::ReleaseRef();
}

//////////////////////////////////////////////////////////////

}  // namespace weave::cancel::sources