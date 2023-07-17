#pragma once

#include <weave/cancel/never.hpp>

#include <weave/futures/syntax/pipe.hpp>

#include <weave/futures/model/evaluation.hpp>

#include <weave/futures/traits/value_of.hpp>

#include <weave/threads/blocking/event.hpp>

#include <optional>

namespace weave::futures {

namespace pipe {

struct [[nodiscard]] ThreadAwait {
  template <Thunk Future>
  class Waiter {
   public:
    using ValueType = typename Future::ValueType;

    explicit Waiter(Future f)
        : eval_(std::move(f).Force(*this)) {
      eval_.Start();
    }

    Result<ValueType> ThreadAwait() {
      event_.Wait();

      return std::move(*res_);
    }

    void Consume(Output<ValueType> o) noexcept {
      res_.emplace(std::move(o.result));
      event_.Set();
    }

    void Cancel(Context) noexcept {
      event_.Set();

      WHEELS_PANIC("futures::ThreadAwait got cancelled!");
    }

    cancel::Token CancelToken() {
      return cancel::Never();
    }

   private:
    threads::blocking::Event event_{};
    std::optional<Result<ValueType>> res_{};
    EvaluationType<Waiter, Future> eval_;
  };

  template <SomeFuture InputFuture>
  Result<traits::ValueOf<InputFuture>> Pipe(InputFuture f) {
    return Waiter(std::move(f)).ThreadAwait();
  }
};

}  // namespace pipe

inline auto ThreadAwait() {
  return pipe::ThreadAwait{};
}

}  // namespace weave::futures
