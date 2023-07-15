#pragma once

#include <weave/cancel/never.hpp>

#include <weave/futures/old_syntax/pipe.hpp>

namespace weave::futures {

namespace pipe {

struct [[nodiscard]] Detach {
  template <SomeFuture Future>
  class Runner final : public IConsumer<typename Future::ValueType> {
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
      delete this;
    }

    void Cancel(Context) noexcept override final {
      WHEELS_PANIC("Cancelled Detach!");
      delete this;
    }

    cancel::Token CancelToken() override final {
      return cancel::Never();
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

inline auto Detach() {
  return pipe::Detach{};
}

}  // namespace weave::futures
