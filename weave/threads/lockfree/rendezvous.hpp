#pragma once

#include <twist/ed/stdlike/atomic.hpp>

namespace weave::threads::lockfree {

class RendezvousStateMachine {
  enum State : int {
    Init = 0,
    Producer = 1,
    Consumer = 2,
    Rendezvous = Producer | Consumer
  };

 public:
  // true means both
  bool Produce() {
    return state_.fetch_or(State::Producer, std::memory_order::acq_rel) ==
           State::Consumer;
  }

  // true means both
  bool Consume() {
    return state_.fetch_or(State::Consumer, std::memory_order::acq_rel) ==
           State::Producer;
  }

 private:
  twist::ed::stdlike::atomic<int> state_{State::Init};
};

// MO proof:
// due to the symmetry it is ether relaxed, sec_cst or acq_rel.
// since just 1 atomic, no need for seq_cst.
// if Produce reads Consumer it enters Rendezvous call in the shared state later
// deleting this object so read of Consumer must have Consume call in hb -> acq
// required. By symmetry rel required as well so acq-rel

}  // namespace weave::threads::lockfree