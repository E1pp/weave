#pragma once

#include <weave/futures/old_model/thunk.hpp>

#include <weave/result/types/unit.hpp>

namespace weave::futures::thunks {

class [[nodiscard]] Never final : public cancel::SignalReceiver {
 public:
  using ValueType = Unit;

  Never() = default;

  // Non-Copyable
  Never(const Never&) = delete;
  Never& operator=(const Never&) = delete;

  // Movable
  Never(Never&&) {
    [[maybe_unused]] int fake = 1;
    fake++;
  };

  Never& operator=(Never&&) = default;

  void Start(IConsumer<Unit>* consumer) {
    consumer_ = consumer;
    consumer_->CancelToken().Attach(this);
  }

  void Cancellable() {
    // No-Op
  }

  void Forward(cancel::Signal signal) override final {
    if (signal.CancelRequested()) {
      consumer_->Cancel(Context{});

      return;
    }

    WHEELS_PANIC("You must not release Never!");
  }

 private:
  IConsumer<ValueType>* consumer_{nullptr};
};

}  // namespace weave::futures::thunks