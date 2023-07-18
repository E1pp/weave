#pragma once

#include <weave/executors/hint.hpp>

#include <weave/fibers/core/fwd.hpp>

namespace weave::fibers {

class FiberHandle;

// Lightweight non-owning handle to a _suspended_ fiber

class FiberHandle {
  friend class Fiber;

 public:
  FiberHandle()
      : FiberHandle(nullptr) {
  }

  static FiberHandle Invalid() {
    return FiberHandle(nullptr);
  }

  cancel::Token CancelToken();

  bool IsValid() const {
    return fiber_ != nullptr;
  }

  // Schedule to the associated scheduler
  void Schedule(
      executors::SchedulerHint hint = executors::SchedulerHint::UpToYou);

  // Switch to this fiber immediately
  // For symmetric transfer
  void Switch();

 private:
  explicit FiberHandle(Fiber* fiber)
      : fiber_(fiber) {
  }

  Fiber* Release();

 private:
  Fiber* fiber_;
};

}  // namespace weave::fibers
