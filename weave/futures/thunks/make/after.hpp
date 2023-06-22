#pragma once

#include <weave/futures/model/thunk.hpp>

#include <weave/result/make/ok.hpp>

#include <weave/timers/delay.hpp>
#include <weave/timers/processor.hpp>
#include <weave/timers/timer.hpp>

namespace weave::futures::thunks {

class [[nodiscard]] After final : public timers::ITimer {
 public:
  using ValueType = Unit;

  explicit After(timers::Delay delay)
      : delay_(delay) {
  }

  // Non-Copyable
  After(const After&) = delete;
  After& operator=(const After&) = delete;

  // Movable
  After(After&& that)
      : delay_(std::move(that.delay_)) {
  }

  After& operator=(After&&) = default;

  ~After() override final = default;

  void Start(IConsumer<Unit>* consumer) {
    consumer_ = consumer;
    delay_.processor_->AddTimer(this);
  }

  void Cancellable() {
    // No-Op
  }

 private:
  timers::Millis GetDelay() override final {
    return delay_.time_;
  }

  void Callback() override final {
    if (consumer_->CancelToken().CancelRequested()) {
      consumer_->Cancel(Context{});
    } else {
      consumer_->Complete(result::Ok());
    }
  }

 private:
  timers::Delay delay_;
  IConsumer<Unit>* consumer_;
};

}  // namespace weave::futures::thunks