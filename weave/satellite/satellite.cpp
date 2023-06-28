#include <weave/satellite/satellite.hpp>
#include <weave/satellite/meta_data.hpp>

#include <twist/ed/local/val.hpp>

namespace weave::satellite {

///////////////////////////////////////////////////////////

twist::ed::ThreadLocal<MetaData> data{};

twist::ed::stdlike::atomic<timers::IProcessor*> global_proc{nullptr};

///////////////////////////////////////////////////////////

MetaData SetContext(executors::IExecutor* exe, cancel::Token token) {
  return data->SetNew(exe, token);
}

void RestoreContext(MetaData old) {
  data->Restore(old);
}

// Cancel Token

void PollToken() {
  data->PollToken();
}

cancel::Token GetToken() {
  return data->token_;
}

// Executor

executors::IExecutor* GetExecutor() {
  return data->executor_;
}

// Timers

void MakeVisible(timers::IProcessor* proc) {
  global_proc.store(proc, std::memory_order::release);
}

timers::IProcessor* GetProcessor() {
  return global_proc.load(std::memory_order::acquire);
}

}  // namespace weave::satellite