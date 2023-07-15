#pragma once

#include <weave/executors/executor.hpp>

#include <weave/futures/syntax/pipe.hpp>

#include <weave/futures/thunks/combine/seq/via.hpp>

#include <weave/futures/old_traits/value_of.hpp>

namespace weave::futures {

namespace pipe {

struct [[nodiscard]] Via {
  Context context_;

  Via(executors::IExecutor& executor, executors::SchedulerHint hint)
      : context_{&executor, hint} {
  }

  template <SomeFuture InputFuture>
  Future<traits::ValueOf<InputFuture>> auto Pipe(InputFuture f) {
    return futures::thunks::Via{std::move(f), context_};
  }
};

}  // namespace pipe

// Future<T> -> Executor -> Future<T>

inline auto Via(
    executors::IExecutor& executor,
    executors::SchedulerHint hint = executors::SchedulerHint::UpToYou) {
  return pipe::Via{executor, hint};
}

}  // namespace weave::futures
