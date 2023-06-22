#include <weave/cancel/sources/strand.hpp>

namespace weave::cancel::sources {

//////////////////////////////////////////////////////////////

// Helper methods

void StrandSource::SetReceiver(SignalReceiver* receiver) {
  state_.store(State::Pointer(receiver), std::memory_order::release);
}

void StrandSource::ClearReceiver() {
  State old = state_.exchange(State::Value(kInit), std::memory_order::acquire);

  if (old.IsPointer()) {
    SignalReceiver* receiver = old.AsPointerTo<SignalReceiver>();

    receiver->Forward(Signal::Release());
  }
}

//////////////////////////////////////////////////////////////

// Memory Management

void StrandSource::AddRef() {
  ref_count_.fetch_add(1, std::memory_order::relaxed);
}

void StrandSource::ReleaseRef() {
  if (ref_count_.fetch_sub(1, std::memory_order::acq_rel) == 1) {
    delete this;
  }
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
    ClearReceiver();
  }

  // Memory Management
  ReleaseRef();
}

//////////////////////////////////////////////////////////////

}  // namespace weave::cancel::sources