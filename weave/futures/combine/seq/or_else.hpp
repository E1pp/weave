#pragma once

#include <weave/futures/old_syntax/pipe.hpp>

#include <weave/futures/thunks/combine/seq/apply.hpp>

#include <weave/result/complete/complete.hpp>

namespace weave::futures {

namespace pipe {

namespace detail {

template <typename Input, typename F>
struct OrElseMapper {
  using InvokeResult = std::invoke_result_t<F, Error>;

  explicit OrElseMapper(F&& fun)
      : fun_(std::move(fun)) {
  }

  bool Predicate(Output<Input>& input) {
    return !static_cast<bool>(input.result);
  }

  InvokeResult Map(Output<Input> input) {
    return fun_(std::move(input.result.error()));
  }

  InvokeResult Forward(Output<Input> input) {
    return std::move(input.result);
  }

 private:
  F fun_;
};

}  // namespace detail

template <typename F>
struct OrElse {
  F fun;

  explicit OrElse(F f)
      : fun(std::move(f)) {
  }

  using InvokeType = result::traits::ValueOf<
      std::invoke_result_t<result::Wrap<Error, F>, Error>>;

  template <SomeFuture InputFuture>
  requires std::is_same_v<traits::ValueOf<InputFuture>, InvokeType>
      Future<InvokeType>
  auto Pipe(InputFuture f) {
    using InputType = typename InputFuture::ValueType;

    auto wrapped = result::Wrap<Error, F>(std::move(fun));

    auto mapper = detail::OrElseMapper<InputType, result::Wrap<Error, F>>(
        std::move(wrapped));

    return futures::thunks::Apply(std::move(f), std::move(mapper));
  }
};

}  // namespace pipe

// Future<T> -> (Error -> Result<T>) -> Future<T>

template <typename F>
auto OrElse(F fun) {
  return pipe::OrElse{std::move(fun)};
}

}  // namespace weave::futures
