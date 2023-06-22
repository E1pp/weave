#pragma once

#include <cstdint>

namespace weave::futures::thunks::detail {

class Dummy {
 public:
  void SetProducerCount(uint32_t) {
  }

  bool TryFulfill() {
    return true;
  }

  bool ProducerDone() {
    return true;
  }
};

}  // namespace weave::futures::thunks::detail