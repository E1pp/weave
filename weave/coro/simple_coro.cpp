#include <weave/coro/simple_coro.hpp>

namespace weave::coro {

void SimpleCoro::Resume() {
  // Coro is done -> Resume does nothing
  if (is_completed_) {
    return;
  }

  // we set who is being active
  SimpleCoro* saved_active = currently_active;
  currently_active = this;
  // we enter context in which routine_ is being executed
  impl_.Resume();
  // coroutine is done or suspended

  currently_active = saved_active;
  // if exception was thrown during the execution exception_container_ has it
  if (exception_container_) {
    std::rethrow_exception(*exception_container_);
  }
}

SimpleCoro* SimpleCoro::Self() {
  return currently_active;
}

void SimpleCoro::Suspend() {
  Self()->impl_.Suspend();
}

bool SimpleCoro::IsCompleted() const {
  return is_completed_;
}

}  // namespace weave::coro
