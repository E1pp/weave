#pragma once

#include <weave/executors/executor.hpp>
#include <weave/executors/task.hpp>

#include <weave/futures/make/just.hpp>
#include <weave/futures/combine/seq/via.hpp>
#include <weave/futures/combine/seq/map.hpp>
#include <weave/futures/run/detach.hpp>

namespace weave::executors {

/*
 * Usage:
 *
 * Submit(thread_pool, [] {
 *   fmt::println("Running on thread pool");
 * });
 *
 */

template <typename F>
void Submit(IExecutor& exe, F fun,
            SchedulerHint hint = SchedulerHint::UpToYou) {
  futures::Just() | futures::Via(exe, hint) | futures::Map(std::move(fun)) |
      futures::Detach();
}

}  // namespace weave::executors