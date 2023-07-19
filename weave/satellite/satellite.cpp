#include <weave/satellite/satellite.hpp>
#include <weave/satellite/meta_data.hpp>

#include <weave/support/constructor_bases.hpp>

#include <twist/ed/stdlike/atomic.hpp>

#include <twist/ed/local/val.hpp>

namespace weave::satellite {

///////////////////////////////////////////////////////////

static twist::ed::ThreadLocal<MetaData> data{};

static twist::ed::stdlike::atomic<timers::IProcessor*> global_proc{nullptr};

///////////////////////////////////////////////////////////

MetaData SetContext(executors::IExecutor* exe, cancel::Token token) {
  return data->SetNew(exe, token);
}

void RestoreContext(MetaData old) {
  data->Restore(old);
}

// Cancel Token

void PollToken() {
  fibers::Fiber* runner = fibers::Fiber::Self();
  if (runner != nullptr && runner->CancelToken().CancelRequested()) {
    throw cancel::CancelledException{};
  }
}

// Executor

executors::IExecutor* GetExecutor() {
  return data->executor_;
}

// Timers

void MakeVisible(timers::IProcessor* proc) {
  global_proc.store(proc, std::memory_order::release);
}

void ResetGlobalProcessor(){
  global_proc.store(nullptr, std::memory_order::relaxed);
}

timers::IProcessor* GetProcessor() {
  return global_proc.load(std::memory_order::acquire);
}

}  // namespace weave::satellite