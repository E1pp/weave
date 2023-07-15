#pragma once

#include <weave/cancel/never.hpp>

#include <weave/futures/syntax/pipe.hpp>

#include <weave/futures/model/evaluation.hpp>

#include <weave/futures/traits/value_of.hpp>

#include <weave/threads/blocking/event.hpp>

#include <optional>

namespace weave::futures {

namespace pipe {

struct [[nodiscard]] Get {
  template <Thunk Future>
  class Waiter {
   public:
    using ValueType = typename Future::ValueType;

    explicit Waiter(Future f)
        : eval_(std::move(f).Force(*this)) {
    }

    Result<ValueType> Get() {
      event_.Wait();

      return std::move(*res_);
    }

    void Complete(Output<ValueType> o) noexcept {
      res_.emplace(std::move(o.result));
      event_.Set();
    }

    void Complete(Result<ValueType> r) noexcept {
      res_.emplace(std::move(r));
      event_.Set();
    }

    void Cancel(Context) noexcept {
      event_.Set();

      WHEELS_PANIC("futures::Get got cancelled!");
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
    return Waiter(std::move(f)).Get();
  }
};

}  // namespace pipe

inline auto Get() {
  return pipe::Get{};
}

}  // namespace weave::futures
