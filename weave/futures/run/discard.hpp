#pragma once

#include <weave/futures/model/evaluation.hpp>

#include <weave/futures/syntax/pipe.hpp>

namespace weave::futures {

namespace pipe {

struct [[nodiscard]] Discard {
  template <SomeFuture Future>
  class Runner final : public cancel::SignalSender {
   public:
    using ValueType = typename Future::ValueType;

    explicit Runner(Future f)
        : eval_(std::move(f).Force(*this)) {
      eval_.Start();
    }

    // Completable<ValueType>
    void Consume(Output<ValueType>) noexcept {
      WHEELS_PANIC("Consume on Discard!");
    }

    // CancelSource
    void Cancel(Context) noexcept {
      delete this;
    }

    cancel::Token CancelToken() {
      return cancel::Token::Fabricate(this);
    }
    
   private:
    // SignalSender
    bool CancelRequested() override final {
      return true;
    }

    bool Cancellable() override final {
      return true;
    }

    void Attach(cancel::SignalReceiver* handler) override final {
      handler->Forward(cancel::Signal::Cancel());
    }

    void Detach(cancel::SignalReceiver*) override final {
      // No-Op
    }

   private:
    EvaluationType<Runner, Future> eval_;
  };

  template <SomeFuture Future>
  void Pipe(Future f) {
    [[maybe_unused]] auto* runner = new Runner(std::move(f));
  }
};

}  // namespace pipe

inline auto Discard() {
  return pipe::Discard{};
}

}  // namespace weave::futures