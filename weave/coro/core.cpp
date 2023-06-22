#include <weave/coro/core.hpp>

namespace weave::coro {

void Coroutine::Resume() {
  // we enter context in which routine_ is being executed
  caller_context_.SwitchTo(callee_context_);
  // coroutine is done or suspended
}

void Coroutine::Suspend() {
  callee_context_.SwitchTo(caller_context_);
}

}  // namespace weave::coro