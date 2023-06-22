#pragma once

#include <weave/cancel/never.hpp>

#include <weave/executors/executor.hpp>

#include <weave/fibers/core/fiber.hpp>

#include <weave/timers/processor.hpp>

namespace weave::satellite {

struct MetaData {
 public:
  MetaData() = default;

  MetaData(const MetaData&) = default;
  MetaData& operator=(const MetaData&) = default;

  MetaData(MetaData&&) = default;
  MetaData& operator=(MetaData&&) = default;

  MetaData SetNew(executors::IExecutor* exe, cancel::Token token) {
    MetaData copy(*this);

    executor_ = exe;
    token_ = token;
    auto* runner = fibers::Fiber::Self();

    if (runner != nullptr) {
      runner->SetScheduler(exe);
    }

    return copy;
  }

  void Restore(MetaData old) {
    *this = old;
  }

  // throws
  void PollToken() {
    if (token_.CancelRequested()) {
      throw cancel::CancelledException{};
    }
  }

 public:
  executors::IExecutor* executor_{nullptr};
  cancel::Token token_{cancel::Never()};
  timers::IProcessor* proc_{nullptr};
};

}  // namespace weave::satellite