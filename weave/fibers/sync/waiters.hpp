#pragma once

#include <weave/fibers/core/handle.hpp>

#include <twist/ed/stdlike/atomic.hpp>
#include <wheels/intrusive/list.hpp>

#include <limits>
#include <optional>

namespace weave::fibers {

////////////////////////////////////////////////////////////////////////

class SimpleWaiter : public wheels::IntrusiveListNode<SimpleWaiter> {
 public:
  void Schedule(
      executors::SchedulerHint hint = executors::SchedulerHint::UpToYou) {
    handle_.Schedule(hint);
  }

  void SetHandle(FiberHandle handle) {
    handle_ = handle;
  }

 private:
  FiberHandle handle_{FiberHandle::Invalid()};
};

using SimpleNode = wheels::IntrusiveListNode<SimpleWaiter>;

[[maybe_unused]] inline SimpleNode* fullptr =
    reinterpret_cast<SimpleNode*>(std::numeric_limits<uintptr_t>::max());

///////////////////////////////////////////////////////////////////////

enum class State : uint64_t { Ready = 0, Used = 1 };

template <typename T>
struct ICargoWaiter : public wheels::IntrusiveListNode<ICargoWaiter<T>> {
  virtual void SetHandle(FiberHandle handle) = 0;
  virtual void Schedule(
      executors::SchedulerHint hint = executors::SchedulerHint::UpToYou) = 0;

  virtual void WriteValue(T) = 0;
  virtual T ReadValue() = 0;

  virtual State MarkUsed() = 0;

  virtual ~ICargoWaiter() = default;
};

///////////////////////////////////////////////////////////////////////

}  // namespace weave::fibers