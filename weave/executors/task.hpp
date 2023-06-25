#pragma once

#include <wheels/intrusive/list.hpp>

#include <function2/function2.hpp>

namespace weave::executors {

struct Task : wheels::IntrusiveListNode<Task> {
  virtual ~Task() = default;

  virtual void Run() noexcept = 0;

  uintptr_t flags = 0;
};

}  // namespace weave::executors
