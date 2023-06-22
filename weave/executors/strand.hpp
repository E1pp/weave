#pragma once

#include <weave/executors/executor.hpp>

#include <twist/ed/stdlike/atomic.hpp>
#include <wheels/intrusive/list.hpp>

#include <memory>

namespace weave::executors {

// Strand / serial executor / asynchronous mutex

class Strand : public IExecutor,
               public Task {
 public:
  using Node = wheels::IntrusiveListNode<Task>;
  using AtomicPtr = twist::ed::stdlike::atomic<Node*>;

  explicit Strand(IExecutor& underlying);

  // Non-copyable
  Strand(const Strand&) = delete;
  Strand& operator=(const Strand&) = delete;

  // Non-movable
  Strand(Strand&&) = delete;
  Strand& operator=(Strand&&) = delete;

  // IExecutor
  void Submit(Task*, SchedulerHint) override;

 private:
  void Run() noexcept override;

 private:
  IExecutor& underlying_;

  std::shared_ptr<AtomicPtr> stack_;

  static inline Node* no_execution_underway{nullptr};
  static inline Node* execution_underway{
      reinterpret_cast<Node*>(std::numeric_limits<uintptr_t>::max())};
};

}  // namespace weave::executors
