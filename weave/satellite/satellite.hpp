#pragma once

#include <weave/timers/fwd.hpp>

// bunch of fwds

namespace weave::executors {
struct IExecutor;
}  // namespace weave::executors

namespace weave::cancel {
class Token;
}  // namespace weave::cancel

namespace weave::satellite {

struct MetaData;

MetaData SetContext(executors::IExecutor*, cancel::Token);

void RestoreContext(MetaData);

// Cancel Token

void PollToken();

// Executors

executors::IExecutor* GetExecutor();

// Timers

void MakeVisible(timers::IProcessor*);

timers::IProcessor* GetProcessor();

}  // namespace weave::satellite