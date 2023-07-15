#pragma once

#include <weave/futures/types/future.hpp>
#include <weave/executors/executor.hpp>

#include <weave/result/traits/value_of.hpp>

#include <weave/futures/make/just.hpp>
#include <weave/futures/combine/seq/and_then.hpp>
#include <weave/futures/combine/seq/via.hpp>

#include <type_traits>

namespace weave::futures {

namespace traits {

template <typename F>
using SubmitT =
    result::traits::ValueOf<std::invoke_result_t<result::Wrap<Unit, F>, Unit>>;

}  // namespace traits

template <typename F>
Future<traits::SubmitT<F>> auto Submit(executors::IExecutor& exe, F fun) {
  return futures::Just() | futures::Via(exe) | futures::AndThen(std::move(fun));
}

}  // namespace weave::futures
