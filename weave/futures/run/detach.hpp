#pragma once

//#include <weave/cancel/never.hpp>

#include <weave/futures/model/evaluation.hpp>

#include <weave/futures/syntax/pipe.hpp>

namespace weave::futures {

namespace pipe {

struct [[nodiscard]] Detach {
  template <SomeFuture Future>
  class Runner {
   public:
    using ValueType = typename Future::ValueType;

    explicit Runner(Future f)
        : eval_(std::move(f).Force(*this)) {
    }

    void Complete(Output<ValueType>) {
      delete this;
    }

    void Complete(Result<ValueType>) {
      delete this;
    }

    // void Cancel(Context) noexcept override final {
    //   WHEELS_PANIC("Cancelled Detach!");
    //   delete this;
    // }

    // cancel::Token CancelToken() override final {
    //   return cancel::Never();
    // }

   private:
    EvaluationType<Runner, Future> eval_;
  };

  template <SomeFuture Future>
  void Pipe(Future f) {
    new Runner(std::move(f));
  }
};

}  // namespace pipe

inline auto Detach() {
  return pipe::Detach{};
}

}  // namespace weave::futures
