#pragma once

#include <sure/context.hpp>
#include <sure/stack/mmap.hpp>

#include <function2/function2.hpp>

#include <type_traits>
#include <exception>
#include <optional>

namespace weave::coro {

//////////////////////////////////////////////////////////////////////

static const size_t kDefaultStackSize = 65536;

//////////////////////////////////////////////////////////////////////

template <typename Function>
class TemplateTrampoline;

// Trimmed Coroutine:
// Some safety checks and methods are gone
// to improve perf
// expected to be ran by some Fiber
// which does everything safely
// if function throws we call std::abort
// doesn't own stack
class Coroutine {
  template <typename Function>
  friend class TemplateTrampoline;

 public:
  template <typename Function>
  requires std::is_nothrow_invocable<Function>::value Coroutine(
      Function function, sure::Stack* stack)
      : stack_(stack) {
    SetupExecutionContext(std::move(function));
  }

  // Unsafe to use after task is done
  // Class is expected to be handled by a Fiber
  // which kills itself when the task is done
  // so no Resume after done UB is possible
  void Resume();

  // Suspend running coroutine
  void Suspend();

 private:
  template <typename Function>
  void SetupExecutionContext(Function routine) {
    auto trampoline = TemplateTrampoline<Function>(std::move(routine), this);

    callee_context_.Setup(stack_->MutView(), &trampoline);

    caller_context_.SwitchTo(callee_context_);
  }

 private:
  sure::Stack* stack_;
  sure::ExecutionContext caller_context_;
  sure::ExecutionContext callee_context_;
  // in theory I can store this context in a static
  // pointer and save it locally in Resume calls
};

template <typename Function>
class TemplateTrampoline : public sure::ITrampoline {
 public:
  explicit TemplateTrampoline(Function runnable, Coroutine* coro)
      : runnable_(std::move(runnable)),
        owner_(coro) {
  }

  [[noreturn]] void Run() noexcept override;

 private:
  Function runnable_;
  Coroutine* owner_;
};

template <typename Function>
[[noreturn]] void TemplateTrampoline<Function>::Run() noexcept {
  // saving Trampoline fields on the coro stack
  Coroutine* owner = owner_;
  Function runnable(std::move(runnable_));

  owner_->callee_context_.SwitchTo(owner_->caller_context_);

  // runnable_ execution starts here
  runnable();

  runnable.~Function();

  owner->callee_context_.ExitTo(owner->caller_context_);
}

}  // namespace weave::coro
