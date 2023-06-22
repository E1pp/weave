#pragma once

#include <weave/futures/thunks/combine/seq/apply.hpp>

#include <weave/futures/syntax/pipe.hpp>

#include <weave/result/complete/complete.hpp>
#include <weave/result/make/err.hpp>

#include <optional>

#include <type_traits>

namespace weave::futures {

namespace pipe {

namespace detail {

template <typename Input, typename F>
struct AndThenMapper {
  using InvokeResult = std::invoke_result_t<F, Input>;

  explicit AndThenMapper(F&& fun)
      : fun_(std::move(fun)) {
  }

  bool Predicate(Output<Input>& input) {
    return static_cast<bool>(input.result);
  }

  InvokeResult Map(Output<Input> input) {
    return fun_(std::move(*input.result));
  }

  InvokeResult Forward(Output<Input> input) {
    return result::Err(std::move(input.result.error()));
  }

 private:
  F fun_;
};

}  // namespace detail

template <typename F>
struct AndThen {
  F fun;

  explicit AndThen(F f)
      : fun(std::move(f)) {
  }

  template <typename T>
  using U = result::traits::ValueOf<std::invoke_result_t<result::Wrap<F>, T>>;

  template <SomeFuture InputFuture>
  Future<U<traits::ValueOf<InputFuture>>> auto Pipe(InputFuture f) {
    using InputType = typename InputFuture::ValueType;

    auto completed = result::Wrap(std::move(fun));

    auto mapper =
        detail::AndThenMapper<InputType, result::Wrap<F>>(std::move(completed));

    return futures::thunks::Apply{std::move(f), std::move(mapper)};
  }
};

}  // namespace pipe

// Future<T> -> (T -> Result<U>) -> Future<U>

template <typename F>
auto AndThen(F fun) {
  return pipe::AndThen{std::move(fun)};
}

}  // namespace weave::futures
