#pragma once

#include <wheels/intrusive/list.hpp>

#include <function2/function2.hpp>

namespace weave::executors {

struct ITask {
  virtual ~ITask() = default;

  virtual void Run() noexcept = 0;
};

struct Task : public ITask, public wheels::IntrusiveListNode<Task> {
  uintptr_t flags = 0;
};

}  // namespace weave::executors
