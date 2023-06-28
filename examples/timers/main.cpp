#include <weave/timers/processors/standalone.hpp>

#include <fmt/core.h>

#include <thread>

using namespace weave; // NOLINT

using namespace std::chrono_literals;

// Timers are object which have a callback and a delay associated with them
// Timers are submitted to a processor with polls these timers
// and runs their callbacks once they are ready

//////////////////////////////////////////////////////////////////////

// Implementing a timer

class SimpleTimer : public timers::ITimer {
  // What is timers delay
  timers::Millis GetDelay() override {
    return 1s;
  }
  
  // Callback 
  void Run() noexcept override {
    fmt::println("Timer expired!");
  }
  
  // Cancellation polling -- more on that in examples/cancel
  bool WasCancelled() override {
    return false;
  }
};

//////////////////////////////////////////////////////////////////////

void ProcessorExample(){
  fmt::println("Processor example");

  timers::StandaloneProcessor proc{};

  SimpleTimer timer{};

  // You can add the timer using AddTimer call
  proc.AddTimer(&timer);
  
  // proc will run a callback while we sleep
  std::this_thread::sleep_for(2s);

  fmt::println("We woke up!");
}

//////////////////////////////////////////////////////////////////////

// Some objects may want to add timer to the processor on their own
// To do that they need a reference to the process
// Struct Delay serves this purpose

struct SubmitOnCommand : timers::ITimer {
 public:
  explicit SubmitOnCommand(timers::Delay del) : del_(del) {
  }

  void SubmitTimer(){
    del_.processor_->AddTimer(this);
  }

 private:
  timers::Millis GetDelay() override {
    return del_.time_;
  }
  
  void Run() noexcept override {
    fmt::println("Timer expired!");
  }
  
  bool WasCancelled() override {
    return false;
  }

 private:
  timers::Delay del_;
};

void DelayExample(){
  fmt::println("Delay example");

  timers::StandaloneProcessor proc{};
  
  // In order to generate a Delay object
  // you can call DelayFromThis method of Processor
  SubmitOnCommand submitter(proc.DelayFromThis(1s));

  // <- Timer is not added to the proc
  submitter.SubmitTimer();
  
  // Timer will be completed while we sleep
  std::this_thread::sleep_for(2s);

  fmt::println("We woke up!");
}

//////////////////////////////////////////////////////////////////////

// Writing DelayFromThis all the time can be very annoying so you
// can use satellite API to simplify the API of your timers

struct SimpleSubmitOnCommand : SubmitOnCommand {
 public:
  // NB nullptr reference potential!
  explicit SimpleSubmitOnCommand(timers::Millis del) : SubmitOnCommand(satellite::GetProcessor()->DelayFromThis(del)) {
  }
};

void MakeGlobalExample(){
  fmt::println("MakeGloabl example");

  timers::StandaloneProcessor proc{};

  // nullptr reference!!!
  // SimpleSubmitOnCommand timer{5ms}; // RE

  // In order to use SimpleSubmitOnCommand you must call the MakeGlobal method first
  proc.MakeGlobal();

  // Now SimpleSubmitOnCommand object can be created
  SimpleSubmitOnCommand timer{5ms};

  timer.SubmitTimer();

  std::this_thread::sleep_for(1s);

  fmt::println("We woke up!");
}

//////////////////////////////////////////////////////////////////////

int main(){
  ProcessorExample();
  DelayExample();
  MakeGlobalExample();
}