#pragma once

#include <weave/futures/model/evaluation.hpp>

#include <weave/result/make/ok.hpp>

#include <weave/support/constructor_bases.hpp>

#include <weave/timers/delay.hpp>
#include <weave/timers/processor.hpp>
#include <weave/timers/timer.hpp>

namespace weave::futures::thunks {

class [[nodiscard]] After final : public support::NonCopyableBase {
 public:
  using ValueType = Unit;

  explicit After(timers::Delay delay)
      : delay_(std::move(delay)) {
  }

  // Movable
  After(After&& that) noexcept
      : delay_(std::move(that.delay_)) {
  }
  After& operator=(After&&) = delete;

 private:
  template <Consumer<ValueType> Cons>
  class EvaluationFor final : public support::PinnedBase,
                              public cancel::SignalReceiver,
                              public timers::ITimer {
    friend class After;

    EvaluationFor(After fut, Cons& cons)
        : cons_(cons),
          delay_(std::move(fut.delay_)) {
    }

   public:
    ~EvaluationFor() override final = default;

    void Start() {
      cons_.CancelToken().Attach(this);

      delay_.processor_->AddTimer(this);
    }

   private:
    // ITimer
    timers::Millis GetDelay() override final {
      return delay_.time_;
    }

    void Run() noexcept override final {
      if (cons_.CancelToken().CancelRequested()) {
        cons_.Cancel(Context{});
      } else {
        Complete(cons_, result::Ok());
      }
    }

    bool WasCancelled() override final {
      return cons_.CancelToken().CancelRequested();
    }

    // SignalReceiver
    void Forward(cancel::Signal signal) override final {
      if (signal.CancelRequested()) {
        delay_.processor_->CancelTimer(this);
      }
    }

   private:
    Cons& cons_;
    timers::Delay delay_;
  };

 public:
  template <Consumer<ValueType> Cons>
  Evaluation<After, Cons> auto Force(Cons& cons) {
    return EvaluationFor<Cons>(std::move(*this), cons);
  }

  void Cancellable() {
    // No-Op
  }

 private:
  timers::Delay delay_;
};

}  // namespace weave::futures::thunks
