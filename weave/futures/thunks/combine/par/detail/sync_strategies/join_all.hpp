#pragma once

#include <weave/threads/lockfree/count_and_flags.hpp>

namespace weave::futures::thunks::detail {

class JoinAllOnHeap {
 public:
  using State = threads::lockfree::CountAndFlags<1>;

  void SetProducerCount(uint32_t count) {
    state_.AddCount(count);
  }

  bool TryFulfill() {
    return !State::ReadFlag<1>(state_.ActivateFlag<1>());
  }

  bool ProducerDone() {
    uint64_t observed = state_.RemoveCount();

    return State::ReadCount(observed) == 1 && !State::ReadFlag<1>(observed);
  }

 private:
  State state_{};
};

class JoinAllOnStack {
 public:
  using State = threads::lockfree::CountAndFlags<1>;

  void SetProducerCount(uint32_t count) {
    state_.AddCount(count);
  }

  bool TryFulfill() {
    return !State::ReadFlag<1>(state_.ActivateFlag<1>());
  }

  bool ProducerDone() {
    uint64_t observed = state_.RemoveCount();

    return State::ReadCount(observed) == 1;
  }

 private:
  State state_{};
};

}  // namespace weave::futures::thunks::detail