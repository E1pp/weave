#pragma once

#include <weave/futures/thunks/combine/seq/apply.hpp>

#include <weave/futures/syntax/pipe.hpp>

#include <weave/result/complete/complete.hpp>
#include <weave/result/make/ok.hpp>
#include <weave/result/make/err.hpp>

#include <type_traits>

namespace weave::futures {

namespace pipe {

namespace detail {

template <typename Input, typename F>
struct MapMapper {
  using ReturnType = std::invoke_result_t<F, Input>;
  using InvokeResult = Result<ReturnType>;

  explicit MapMapper(F&& fun)
      : fun_(std::move(fun)) {
  }

  bool Predicate(Output<Input>& input) {
    return static_cast<bool>(input.result);
  }

  InvokeResult Map(Output<Input> input) {
    return result::Ok(fun_(std::move(*input.result)));
  }

  InvokeResult Forward(Output<Input> input) {
    return result::Err(std::move(input.result.error()));
  }

 private:
  F fun_;
};

}  // namespace detail

template <typename F>
struct Map {
  F fun;

  explicit Map(F f)
      : fun(std::move(f)) {
  }

  template <typename Input>
  using Completed = result::Complete<Input, F>;

  template <typename Input>
  using U = std::invoke_result_t<Completed<Input>, Input>;

  template <SomeFuture InputFuture>
  Future<U<traits::ValueOf<InputFuture>>> auto Pipe(InputFuture f) {
    using InputType = typename InputFuture::ValueType;

    auto completed = Completed<InputType>(std::move(fun));

    auto mapper = detail::MapMapper<InputType, Completed<InputType>>(
        std::move(completed));

    return futures::thunks::Apply(std::move(f), std::move(mapper));
  }
};

}  // namespace pipe

// Future<T> -> (T -> U) -> Future<U>

template <typename F>
auto Map(F fun) {
  return pipe::Map{std::move(fun)};
}

}  // namespace weave::futures
