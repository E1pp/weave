#include <weave/satellite/satellite.hpp>
#include <weave/satellite/meta_data.hpp>

#include <twist/ed/local/var.hpp>

namespace weave::satellite {

///////////////////////////////////////////////////////////

twist::ed::ThreadLocal<MetaData> data{};

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
  data->proc_ = proc;
}

timers::IProcessor* GetProcessor() {
  return data->proc_;
}

}  // namespace weave::satellite