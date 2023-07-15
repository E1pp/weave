#pragma once

#include <weave/futures/old_syntax/pipe.hpp>

namespace weave::futures {

namespace pipe {

struct [[nodiscard]] Discard {
  template <SomeFuture Future>
  class Runner final : public IConsumer<typename Future::ValueType>,
                       public cancel::SignalSender {
   public:
    using ValueType = typename Future::ValueType;

    explicit Runner(Future f)
        : future_(std::move(f)) {
    }

    void Start() {
      future_.Start(this);
    }

   private:
    void Consume(Output<ValueType>) noexcept override final {
      WHEELS_PANIC("Consume on Discard!");
    }

    void Cancel(Context) noexcept override final {
      delete this;
    }

    cancel::Token CancelToken() override final {
      return cancel::Token::Fabricate(this);
    }

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
    Future future_;
  };

  template <SomeFuture Future>
  void Pipe(Future f) {
    auto* runner = new Runner(std::move(f));
    runner->Start();
  }
};

}  // namespace pipe

inline auto Discard() {
  return pipe::Discard{};
}

}  // namespace weave::futures