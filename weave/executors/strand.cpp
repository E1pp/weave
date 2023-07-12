#include <weave/executors/strand.hpp>
#include <weave/executors/submit.hpp>

#include <algorithm>
#include <utility>

namespace weave::executors {

Strand::Strand(IExecutor& underlying)
    : underlying_(underlying),
      stack_(std::make_shared<AtomicPtr>(nullptr)) {
}

void Strand::Submit(Task* task, SchedulerHint) {
  Node* former_top = stack_->load(std::memory_order::relaxed);
  // reading foul former_top doesn't cause anything so we are free to set
  // relaxed

  do {
    task->next_ = former_top;
    // seq_cst because it can be an external submit to ThreadPool
    // which manipulates with TaskFlags thus must be cross_var hb
  } while (!stack_->compare_exchange_weak(former_top, task,
                                          std::memory_order::seq_cst,
                                          std::memory_order::relaxed));

  if (former_top == no_execution_underway) {
    underlying_.Submit(this);
  }
  // failure never implies access to anything-> relaxed
  // push needs to be in hb with PopAll so rel here and acq in PopAll
}

// is only called by one thread
// is only called when top_ != fake_node_
void Strand::Run() noexcept {
  // save resourse state
  auto preserved_stack = stack_;

  // steal stack
  Node* stolen_queue_head =
      preserved_stack->exchange(execution_underway, std::memory_order::acquire);
  // reading fullptr never let's into critical section so no need for rel here

  {  // reverse stack
    while (stolen_queue_head->next_ != execution_underway &&
           stolen_queue_head->next_ != no_execution_underway) {
      stolen_queue_head->next_->prev_ = stolen_queue_head;

      stolen_queue_head = stolen_queue_head->next_;
    }
  }

  // do some tasks

  while (stolen_queue_head != nullptr) {
    Node* task = stolen_queue_head;

    stolen_queue_head = stolen_queue_head->prev_;
    task->prev_ = task->next_ = nullptr;

    task->AsItem()->Run();
  }

  Node* execution_copy = execution_underway;
  // Always an internal submit thus can be kept at release
  if (!preserved_stack->compare_exchange_strong(
          execution_copy, no_execution_underway, std::memory_order::release,
          std::memory_order::relaxed)) {
    underlying_.Submit(this);
  }
  // reading nullptr submits another Run call which needs to be in hb with this
  // one
}

}  // namespace weave::executors
