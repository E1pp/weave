# Weave
Concurrency for C++

# Branch Status:

 - [TaggedBuffer] Rework – Not done

 - [futures]:
    - [model]:
      - [Thunk] – Final
      - [Evaluation] – Final
      - [Context]/[Output] – Final
      - [Consumer] – Final
    - [make]:
      - [Just] – Done
      - [Value] – Done
      - [Failure] – Done
      - [Submit] – Done
      - [Contract] – Done
      - [Never] – Done
      - [After] – Not Done
    - [combine/seq]
      - [AndThen]/[Map]/[OrElse] – Done
      - [Via] – Done
      - [Flatten/FlatMap] – Done
      - [OnCancel]/[OnSuccess]/[Anyway] – Done
      - [Start] – Done
      - [Box] – Done
      - [WithTimeout] – Not Done
      - [Fork] – Not Done
    - [combine/par]
      - [default/tuple] – Not Done
        - [First] – Done
        - [All] – Not Done
        - [Select] – Done
        - [Quorum] – Done
      - [default/vector] – Not Done
        - [First] – Done
        - [All] – Not Done
        - [Quorum] – Done
      - [no_alloc/tuple] – Not Done
      - [no_alloc/vector] – Not Done
    - [run]
      - [Get] – Done
      - [Detach] – Done
      - [Await] – Not Done
      - [Discard] – Done
    - [syntax]
      - [pipe] – Done
      - [bang] – Done
      - [or] – Done
      - [and] – Not Done
    - [types]
      - [eager] – Done
      - [boxed] – Done
    - [traits]
      - [cancellable] – Not Done

  - [executors/submit] – Done

## Features

- Scalable work-stealing scheduler – `executors::tp::fast::ThreadPool`
- Transparent stackful fibers via `executors::fibers::ThreadPool` with automatic pooling
- Functional futures
- Futures are lazy ⟹ allocation and synchronization amount is minimized (usually to zero)
- Transparent cooperative cancellation, wait-free for sequential composition and lock-free for parallel composition
- Structured concurrency via parallel combinators ('All', 'First', etc)
- Deterministic stress-tests with fault-injection via [`Twist`](https://gitlab.com/Lipovsky/twist) framework

## Contents

- [Executors](weave/executors)
  - Thread Pools (`ThreadPool`)
    - `tp::compute::ThreadPool` with shared blocking queue for independent CPU-bound tasks
    - Scalable work-stealing `tp::fast::ThreadPool` for fibers / stackless coroutines (IO-bound tasks)
  - `Strand` (asynchronous mutex)
  - `ManualExecutor` for deterministic testing
  - Transparent fibers (`fibers`)
    - `fibers::ThreadPool`
    - `fibers::ManualExecutor`
- [Futures](weave/futures)
  - Types (`types`)
    - concepts `SomeFuture`, `Future<T>`, `EagerFuture`
    - `BoxedFuture<T>`
  - Constructors (`make`)
    - `After`
    - `Contract` + `Promise<T>`
    - `Failure`
    - `Just`
    - `Never`
    - `Submit`
    - `Value`
  - Combinators (`combine`)
    - Sequential composition (`seq`)
      - `Map` / `AndThen` / `OrElse` – monadic mappers for monadic value type `T`
      - `Flatten` – flattens nested futures (for asynchronous mappers)
      - `FlatMap` = `Map` + `Flatten`
      - `Via` – sets executor and/or hint for following mappers
      - `Box` – erases concrete `Future` (`Thunk`) type
      - `Start` (or `Force`) – converts to `EagerFuture`, starts operation
      - `Fork<N>` – splits future value into N copies
      - `OnComplete` / `OnCancel` / `Anyway` – add side effect based on what happened to the underlying future
      - `WithTimeout` – attaches timeout
    - Parallel composition (`par`)
      - `All`/`Both`
      - `First`
      - `Select` (`std::variant` alternative to `First`)
      - `Quorum` (blocking)
  - Terminators (`run`)
    - `Await` – synchronously unwraps `Result` from future
    - `Detach` – moves future on the stack and starts it without cancelling it
    - `Discard` – moves future on the stack, starts it and cancels it
  - Operators (`syntax`)
    - _pipe_: `Just() | Via(pool) | Apply([]{})`
    - _bang_: `!f` ~ `f | Start()`
    - _or_: `f or g` ~ `FirstOf(f, g)`
    - _sum_: `f + g` ~ `All(f, g)`
- [Stackful Fibers](weave/fibers)
  - Scheduling (`sched`)
    - `Yield`
    - `SleepFor`
  - Synchronization (`sync`)
    - `Mutex` (lock-free)
    - `OneShotEvent` (lock-free)
    - `WaitGroup` (lock-free)
    - Buffered `Channel<T>` + `Select`
    - Lock-free unbuffered `experimental::Channel<T>`
- [Timers](weave/timers)

## Examples

- [Futures](examples/futures/main.cpp)
- [Executors](examples/executors/main.cpp)
- [Fibers](examples/fibers/main.cpp)
- [Cancellation](examples/cancel/main.cpp)
- [Timers](examples/timers/main.cpp)

Unit [tests](tests) contain most of the common-sense uses of this library and push semantics of its components to the limits. You should consider reading them in order to fully understand how to use the weave library.

## References

This library was created based on solutions for problems from the MIPT [concurrency course](https://gitlab.com/Lipovsky/concurrency-course).

## Requirements

|                  | _Supported_       |
|------------------|-------------------|
| Architecture     | x86-64            |
| Operating System | Linux             |
| Compiler         | Clang++ (≥ 14)    |

## Dependencies

- [Wheels](https://gitlab.com/Lipovsky/wheels) – core components, test framework
- [Sure](https://gitlab.com/Lipovsky/sure) – context switching for stackful fibers / coroutines
- [Twist](https://gitlab.com/Lipovsky/twist) – fault injection framework for concurrency
- [expected](https://github.com/TartanLlama/expected) - error handling
- [Mimalloc](https://github.com/microsoft/mimalloc) – memory allocation
- [function2](https://github.com/Naios/function2.git) – drop-in replacement for `std::function`
- [fmt](https://github.com/fmtlib/fmt.git) – formatted output


