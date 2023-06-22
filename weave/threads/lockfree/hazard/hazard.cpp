#include <weave/threads/lockfree/hazard/manager.hpp>

#include <twist/ed/local/ptr.hpp>

namespace weave::threads::hazard {

TWISTED_THREAD_LOCAL_PTR(ThreadState, local_state)

Manager* Manager::Get() {
  static Manager instance;
  return &instance;
}

Mutator Manager::MakeMutator(size_t num_hazards) {
  if (local_state == nullptr || local_state->hazards_.size() < num_hazards) {
    // if thread uses Manager for another purpose which needs more hazards,
    // we must create a new node as well
    if (local_state != nullptr) {
      local_state->Collect();
    }

    local_state = new ThreadState(num_hazards);

    Push(local_state);
  }

  return Mutator(local_state);
}

// must have every other op in hb with this one via user
void Manager::Collect() {
  auto* stack_head = states_.exchange(nullptr, std::memory_order::acquire);

  while (stack_head != nullptr) {
    stack_head->Collect();

    delete stack_head->hazards_.data();
    delete std::exchange(stack_head, stack_head->next_);
  }
}

}  // namespace weave::threads::hazard