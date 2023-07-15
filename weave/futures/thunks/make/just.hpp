#pragma once

#include <weave/futures/old_model/thunk.hpp>

#include <weave/result/make/ok.hpp>

namespace weave::futures::thunks {

class [[nodiscard]] Just {
 public:
  using ValueType = Unit;

  Just() = default;

  // Non-Copyable
  Just(const Just&) = delete;
  Just& operator=(const Just&) = delete;

  // Movable
  Just(Just&&) {
    [[maybe_unused]] int fake = 1;
    fake++;
  };

  Just& operator=(Just&&) = default;

  void Start(IConsumer<Unit>* consumer) {
    if (consumer->CancelToken().CancelRequested()) {
      consumer->Cancel(Context{});
    } else {
      consumer->Complete(result::Ok());
    }
  }

  void Cancellable() {
    // No-Op
  }
};

}  // namespace weave::futures::thunks
