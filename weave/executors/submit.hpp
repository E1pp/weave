#pragma once

#include <weave/executors/executor.hpp>
#include <weave/executors/task.hpp>

// #include <weave/futures/make/just.hpp>
// #include <weave/futures/combine/seq/via.hpp>
// #include <weave/futures/combine/seq/map.hpp>
// #include <weave/futures/run/detach.hpp>

namespace weave::executors {

/*
 * Usage:
 *
 * Submit(thread_pool, [] {
 *   fmt::println("Running on thread pool");
 * });
 *
 */

// template <typename F>
// void Submit(IExecutor& exe, F fun,
//             SchedulerHint hint = SchedulerHint::UpToYou) {
//   auto wrapped = [f = std::move(fun)](Unit) mutable -> Status {
//     f();
//     return result::Ok();
//   };

//   futures::Just() | futures::Via(exe, hint) | futures::Map(std::move(wrapped)) |
//       futures::Detach();
// }

template <typename F>
struct UserTask : public Task {
  explicit UserTask(F&& fun) : fun_(std::move(fun)){
  }
  
  void Run() noexcept {
    std::move(fun_)();
    delete this;
  }

 F fun_;
};

template <typename F>
void Submit(IExecutor& exe, F fun,
            SchedulerHint hint = SchedulerHint::UpToYou) {
  auto* task = new UserTask(std::move(fun));

  exe.Submit(task, hint);
}

}  // namespace weave::executors