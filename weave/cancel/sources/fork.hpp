#pragma once

#include <weave/cancel/token.hpp>

#include <weave/cancel/sources/strand.hpp>

#include <twist/ed/stdlike/atomic.hpp>

namespace weave::cancel::sources {

template <size_t Total, typename Seq>
struct ForkVariadicBase;

template <size_t Index, size_t Total>
struct SignalReceiverLeaf;

// Source for 1 consumer
// Receiver for multiple producers
// Cancelled
template <size_t NumTines>
class ForkSource
    : public ForkVariadicBase<NumTines, std::make_index_sequence<NumTines>>,
      public StrandSource {
 public:
  ForkSource() = default;

  // Non-copyable
  ForkSource(const ForkSource&) = delete;
  ForkSource& operator=(const ForkSource&) = delete;

  // Non-movable
  ForkSource(ForkSource&&) = delete;
  ForkSource& operator=(ForkSource&&) = delete;

  ~ForkSource() override = default;

  void Forward(Signal signal) override {
    if (!signal.CancelRequested()) {
      should_cancel_.store(false);
      StrandSource::ClearReceiver(StrandSource::kAnyOne);
    }

    if (bool should_cancel = ReleaseTine()) {
      // All signals were cancel requests
      StrandSource::Forward(signal);
    } else {
      StrandSource::ReleaseRef();
    }
  }

  template <size_t Index>
  SignalReceiver* AsLeaf() {
    return static_cast<SignalReceiverLeaf<Index, NumTines>*>(this);
  }

 private:
  bool ReleaseTine() {
    return active_tines_.fetch_sub(1) == 1 && should_cancel_.load();
  }

 private:
  twist::ed::stdlike::atomic<size_t> active_tines_{NumTines};
  twist::ed::stdlike::atomic<bool> should_cancel_{true};
};

//////////////////////////////////////////////////////////////////////////

template <size_t Total, typename Seq>
struct ForkVariadicBase {};

template <size_t Total, size_t... Index>
struct ForkVariadicBase<Total, std::index_sequence<Index...>>
    : SignalReceiverLeaf<Index, Total>... {};

//////////////////////////////////////////////////////////////////////////

template <size_t Index, size_t Total>
struct SignalReceiverLeaf : SignalReceiver {
  using Derived = ForkSource<Total>;

  SignalReceiverLeaf() = default;

  ~SignalReceiverLeaf() override = default;

  void Forward(Signal signal) override {
    static_cast<Derived*>(this)->Forward(signal);
  }
};

}  // namespace weave::cancel::sources