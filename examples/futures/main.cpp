#include <weave/executors/thread_pool.hpp>
#include <weave/executors/fibers/manual.hpp>

#include <weave/futures/make/after.hpp>
#include <weave/futures/make/contract.hpp>
#include <weave/futures/make/failure.hpp>
#include <weave/futures/make/just.hpp>
#include <weave/futures/make/never.hpp>
#include <weave/futures/make/submit.hpp>
#include <weave/futures/make/value.hpp>

#include <weave/futures/combine/seq/and_then.hpp>
#include <weave/futures/combine/seq/anyway.hpp>
#include <weave/futures/combine/seq/box.hpp>
#include <weave/futures/combine/seq/flatten.hpp>
#include <weave/futures/combine/seq/fork.hpp>
#include <weave/futures/combine/seq/map.hpp>
#include <weave/futures/combine/seq/or_else.hpp>
#include <weave/futures/combine/seq/start.hpp>
#include <weave/futures/combine/seq/via.hpp>
#include <weave/futures/combine/seq/with_timeout.hpp>

#include <weave/futures/combine/par/first.hpp>
#include <weave/futures/combine/par/all.hpp>

#include <weave/futures/run/detach.hpp>
#include <weave/futures/run/await.hpp>

#include <weave/futures/old_traits/is_lazy.hpp>

#include <weave/timers/processors/standalone.hpp>

#include <fmt/core.h>

#include <atomic>
#include <chrono>

using namespace weave; // NOLINT

using namespace std::chrono_literals;

//////////////////////////////////////////////////////////////////////

// Future represents result from (potentially) asynchronous operation

// Internally (and sometimes externally) futures do not work with raw value.
// Instead they use Result<T> which is ~std::expected<T, std::error_code>

// Futures are lazy by default, meaning that the underlying calculation is
// not started unless result is requested explicitly

// Futures API allows doing most of the things you are able 
// to do directly with weave lib with (almost) zero overhead.
// At the same time, usage of futures provides extra guarantees
// for lifetimes and also allows writing code in a declarative style

//////////////////////////////////////////////////////////////////////

void SubmitExample(){
  fmt::println("Submit example");

  // You have already seen futures::Submit
  // the most effecient way of submitting a lambda

  executors::ThreadPool pool{4};

  pool.Start();

  auto future = futures::Submit(pool, []{
    fmt::println("Hello from thread pool!");
  });

  fmt::println("<- no task submitted here");

  std::move(future) | futures::Await();

  // <- lambda was first submitted and then executed
  // Since Await blocks current thread and waits for
  // compelition, we don't need to allocate memory

  pool.Stop();
}

//////////////////////////////////////////////////////////////////////

void ConceptExample(){
  fmt::println("Concept example");

  executors::ThreadPool pool{4};

  pool.Start();

  // Annotate future as Future<int> using concepts
  futures::Future<int> auto future = futures::Submit(pool, []{
    fmt::println("Hello from thread pool!");
    return 42;
  });

  std::move(future) | futures::Await();

  pool.Stop();  
}

//////////////////////////////////////////////////////////////////////

void PipelineExample(){
  fmt::println("Pipeline example");

  // You can use pipeline "|" operator 
  // to combine future with "combinators"
  bool ready = false;
  
  auto pipeline = futures::Value(42) | futures::Map([&](int v){
    ready = true;
    fmt::println("Mapping value {}", v);
    return 37;
  });

  assert(!ready);

  // Await for the result
  auto res = std::move(pipeline) | futures::Await();

  fmt::println("Mapped value {}", *res);
}

//////////////////////////////////////////////////////////////////////

void RecoverExample(){
  fmt::println("Recover example");

  // Sometimes your operation may fail
  // producing an error

  // futures::Failure emulates this process
  auto fail = futures::Failure<int>(std::make_error_code(std::errc::timed_out));

  // Combinator AndThen (and Map) does your code only 
  // when operation was finished without errors. 

  // Combinator OrElse handles errors that could have
  // happened

  auto handled = std::move(fail) | futures::AndThen([](int){
    // Skipped
    return 1;
  }) | futures::OrElse([](Error){
    fmt::println("Got and error!");
    return 37;
  });

  // Now you can await your output
  auto res = std::move(handled) | futures::Await();

  fmt::println("Recovered {}", *res);
}

//////////////////////////////////////////////////////////////////////

void ContractExample(){
  fmt::println("Contract example");

  // Contract is pair of future and promise
  auto [f, p] = futures::Contract<int>();

  // Future from contract is an Eager future -- at any point in time
  // (before future is started or after), result might be present
  // This requires dynamic allocation
  f.ImEager();

  // Promise has methods Set, SetValue, SetError
  // allowing its owner to fulfill the contract.
  std::move(p).SetValue(42);

  auto res = std::move(f) | futures::Await();

  fmt::println("Contract value {}", *res);
}

//////////////////////////////////////////////////////////////////////

void StartExample(){
  fmt::println("Start example");

  executors::ThreadPool pool{4};

  pool.Start();

  // You can turn lazy future into an eager one using futures::Start
  
  auto f = futures::Submit(pool, []{
    fmt::println("Running on a pool!");
    return 42;
  }) | futures::Start();
  // <- Task was submitted to a pool

  f.ImEager();

  // Combining eager future with something other than start
  // results in a lazy future
  auto g = std::move(f) | futures::AndThen([](int){
    return 42;
  });

  static_assert(futures::traits::Lazy<decltype(g)>);
  
  std::move(g) | futures::Await();

  pool.Stop();
}

//////////////////////////////////////////////////////////////////////

void BoxExample(){
  fmt::println("Box example");

  // Since future is a concept it might be convenient 
  // to erase its type

  auto f = futures::Just() | futures::AndThen([](Unit) {

  }) | futures::Map([](Unit){
    fmt::println("Hello from map!");
    return 42;
  });

  // Type is erased
  futures::BoxedFuture<int> g = std::move(f) | futures::Box();

  auto res = std::move(g) | futures::Await();

  fmt::println("Result is {}", *res);
}

//////////////////////////////////////////////////////////////////////

void AutoBoxingExample(){
  fmt::println("Autoboxing example");

  auto f = futures::Just() | futures::AndThen([](Unit) {

  }) | futures::Map([](Unit){
    fmt::println("Hello from map!");
    return 42;
  });

  // You don't have to call futures::Box explicitly
  // Just write the type correctly
  futures::BoxedFuture<int> g = std::move(f);

  auto res = std::move(g) | futures::Await();

  fmt::println("Result is {}", *res);  
}

//////////////////////////////////////////////////////////////////////

void FlattenExample(){
  fmt::println("Flatten example");

  executors::ThreadPool pool{4};

  pool.Start();

  // Sometimes you want to asyncronously create an
  // asyncronous operation and retrieve result from it

  auto f = futures::Submit(pool, [&]{
    fmt::println("Async submit!");

    return futures::Submit(pool, []{
      return 42;
    });
  });

  // You could directly await for outer future to create an inner future
  // and then await the inner future as well

  /*
  auto inner_future = std::move(f) | futures::Await();

  Result<int> res = std::move(*inner_future) | futures::Await();
  */

  // But you can await the inner future directly if you use Flatten
  futures::Future<int> auto flat = std::move(f) | futures::Flatten();

  auto result = std::move(flat) | futures::Await();

  fmt::println("Result is {}", *result);

  pool.Stop();
}

//////////////////////////////////////////////////////////////////////

void FirstExample(){
  fmt::println("First example");

  // If you want to wait only for the first operation to complete
  // you can use combinator First

  executors::fibers::ManualExecutor manual;

  auto fast = futures::Value(42);

  // Submitted to manual but not ran
  auto slow = futures::Submit(manual, []{
    return 37;
  }) | futures::Start();

  slow.ImEager();

  // Take the first ready future
  auto first = futures::First(std::move(fast), std::move(slow));

  // Control returns immediatelly
  auto result = std::move(first) | futures::Await();

  fmt::println("Result is {}, there are still {} tasks in manual", *result, manual.TaskCount());

  manual.Drain();

  manual.Stop();
}

//////////////////////////////////////////////////////////////////////

void ForkExample(){
  fmt::println("Clone example");

  executors::ThreadPool pool{4};
  pool.Start();

  // You may want to create several futures
  // carrying the same result (assuming it is copyable)
  // to do different things with them

  auto [left, right] = futures::Submit(pool, []{
    fmt::println("Hi!");
    return 42;
  }) | futures::Fork<2>();

  fmt::println("{} <- Right", *(std::move(right) | futures::Await()));

  fmt::println("{} <- Left", *(std::move(left) | futures::Await()));

  pool.Stop();
}

//////////////////////////////////////////////////////////////////////

void AfterExample(){
  fmt::println("After example");

  timers::StandaloneProcessor proc{};
  proc.MakeGlobal();

  // Future will be fulfilled with Unit 1s after it was started
  auto after1s = futures::After(1s);

  auto print_later = std::move(after1s) | futures::Map([](Unit){
    fmt::println("1 second later...");
  });

  std::move(print_later) | futures::Await();
}

//////////////////////////////////////////////////////////////////////

void WithTimeoutExample(){
  fmt::println("Timeout example");

  timers::StandaloneProcessor proc{};
  proc.MakeGlobal();

  auto very_slow = futures::Never();

  // You can set a timeout to the future
  auto with_timeout = std::move(very_slow) | futures::WithTimeout(1s);

  std::move(with_timeout) | futures::OrElse([](Error){
    fmt::println("Timed out!");
  }) | futures::Await();

  // <- 1 second later
}

//////////////////////////////////////////////////////////////////////

int main() {
  SubmitExample();
  ConceptExample();
  PipelineExample();
  RecoverExample();
  ContractExample();
  StartExample();
  BoxExample();
  AutoBoxingExample();
  FlattenExample();
  FirstExample();
  ForkExample();
  AfterExample();
  WithTimeoutExample();
}
