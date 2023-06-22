#include <weave/threads/lockfree/hazard/manager.hpp>
#include <weave/threads/lockfree/hazard/thread_state.hpp>

namespace weave::threads::hazard {

void ThreadState::Collect() {
  ThreadState* head = Manager::Get()->StackHead();

  // mark for deletion
  for (auto& retiree : retirees_) {
    retiree.should_delete_ = true;
  }

  // unmark those who are hazards
  while (head != nullptr) {
    for (auto& hazard : head->hazards_) {
      auto current_hazard = hazard.load();

      for (auto& retiree : retirees_) {
        if (retiree.ptr_ == current_hazard) {
          retiree.should_delete_ = false;
        }
      }
    }

    head = head->next_;
  }

  // delete
  size_t num_remaining{0};
  for (auto& retiree : retirees_) {
    if (!retiree.TryDelete()) {
      retirees_[num_remaining++] = retiree;
    }
  }

  while (retirees_.size() > num_remaining) {
    retirees_.pop_back();
  }
}

}  // namespace weave::threads::hazard