
# Weave
Concurrency for C++

## Features

- Scalable work-stealing scheduler – `executors::tp::fast::ThreadPool`
- Transparent stackful fibers via `executors::fibers::ThreadPool` with automatic pooling
- Functional futures with almost everything deduced in compile-time
- Futures are lazy ⟹ allocation and synchronization is minimized (usually to zero)
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
      - `Map` / `AndThen` / `OrElse` – monadic mappers for `Result<T>`
      - `Flatten` – flattens nested futures (for asynchronous mappers)
      - `FlatMap` = `Map` + `Flatten`
      - `Via` – sets executor and/or hint for following mappers
      - `Box` – erases concrete `Future` (`Thunk`) type
      - `Start` (or `Force`) – converts to `EagerFuture`, starts operation
      - `Fork<N>` – splits future value into N copies
      - `OnComplete` / `OnCancel` / `Anyway` – add side effect
      - `WithTimeout` – attaches timeout
    - Parallel composition (`par`)
      - `All`/`Both`
      - `First`
      - `Select` (`std::variant` alternative to `First`)
      - `Quorum` (blocking)
      - `no_alloc` versions which saves up allocations at the cost of less intuitive semantics
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

## Docs

### Getting Started

This library has a lot of Unit [tests](tests) which will probably explain how everything works much better than I ever would, so you should really consider reading them. Below are the most important things you should know about `weave`. After that there is a section about advanced features `weave` has.

#### 1. First  look at `executors`
Executor is an abstraction which launches user's code. We have a function `executors::Submit` which can be used to send lambdas to any executor. 

##### `ThreadPool`
`ThreadPool` is designed to run tasks asynchronously
```cpp
executors::ThreadPool pool{/*threads=*/4};

pool.Start(); // We must start the pool

executors::Submit(pool, []{
	fmt::println("Running on a ThreadPool!");
}); // Task is submitted and eventually ran by another thread

pool.WaitIdle(); // Wait for ThreadPool to finish every task it has

pool.Stop(); // ~= thread::join
```

##### `ManualExecutor`
`ManualExecutor` can be used to run tasks manually which can prove helpful with deterministic testing
```cpp
executors::ManualExecutor manual;
 
executors::Submit(manual, []{
	 fmt::println("Step 1");
});

executors::Submit(manual, []{
	fmt::println("Step 2");
});

manual.RunNext(); // prints "Step 1" and returns true

manual.Drain(); // prints "Step 2" and returns 1
```
 
#### 2. "Forget about `executors::Submit` " or "First look at `futures`"
Futures are object which represent represent a value, which might not be ready at the moment, and the underlying computation which would return this value. Futures in `weave` are lazy, meaning that they will postpone any kind of evaluations until they known how exactly their value will be used. This model allows us avoid numerous dynamic allocations and vtable lookups since we always know (almost) every consumer at compile-time. Now, let's look how we can come up with a better API for submitting lambdas.

##### `Submit`
Using `executors::Submit` is very simple but inefficient. It uses type erasure which can be avoided. Submitted task can be represented as future, also known as `futures::Submit`:
```cpp
executors::ThreadPool pool{/*threads=*/4};

pool.Start(); // We must start the pool

auto f = futures::Submit(pool, []{
	fmt::println("Running on a ThreadPool!");
}); // Task is not yet submitted! No allocations are done
...
```

Next step in our replacement of `executors::Submit` would be actually getting task to run. We can do it in several ways:

##### `Detach`
We can let the task run on its own without getting the return value using `futures::Detach`:
```cpp
executors::ThreadPool pool{/*threads=*/4};

pool.Start(); // We must start the pool

auto f = futures::Submit(pool, []{
	fmt::println("Running on a ThreadPool!");
}); // Task is not yet submitted! No allocations are done

std::move(f) | futures::Detach(); // Future is moved on heap and lambda is submitted

pool.WaitIdle();

pool.Stop();
```
But wait! We are still allocating the memory, so what's the point of this? Indeed, doing `Detach` on `Submit` does exactly the same thing that `executors::Submit` does. However, we can optimize that using the fact that we are synchronously waiting for task to complete, which leads us to

#####  `Await`

We can synchronously wait for task to be executed using `futures::Await`:
```cpp
executors::ThreadPool pool{/*threads=*/4};

pool.Start(); // We must start the pool

auto f = futures::Submit(pool, []{
	fmt::println("Running on a ThreadPool!");
	return result::Ok(42); // ???
}); // Task is not yet submitted! No allocations are done

auto result = std::move(f) | futures::Await();
// lambda was already executed at this point
pool.Stop();
```
In this example there are zero memory allocations and exactly two vtable lookups: who is the `IExecutor` and who is the `Task`.  

Now you might be wondering what types are `f` and `result` which we are going to discuss now.

##### `Result<T>` and `Future<T>`
The example above can be rewritten using type `Result<T>` and concept `Future<T>` like this
```cpp
executors::ThreadPool pool{/*threads=*/4};

pool.Start(); // We must start the pool

Future<int> auto f = futures::Submit(pool, []{
	fmt::println("Running on a ThreadPool!");
	return result::Ok(42);
}); // Task is not yet submitted! No allocations are done

Result<int> result = std::move(f) | futures::Await();
// lambda was already executed at this point
pool.Stop();
```
`Result<T>` represents and object which be either the value itself or an error -- just in case the evaluation aborts for some reason. The underlying type is `tl::expected<T, std::error_code>` allowing you to use `expected` library API on these objects. `weave` provides convenient functions `result::Ok` and `result::Err` for return types in futures. 

We use concept `Future<T>` instead of concrete type because we store a lot of information in future's using some template metaprogramming magic. Now, this can be very incovenient if you are writing an interface which accepts the future or you simply want to store a bunch of them in an STL-container. In this case you can use

##### `Box`
`Box` allow you to erase futures type. Suppose you have two futures:
```cpp
Future<int> auto first = futures::Submit(pool, []{
	return result::Ok(42);
});

Future<int> auto second = futures::Submit(pool, []{
	fmt::println("I'm different!");
	return result::Ok(37);
});
```
And you want to write a function, which takes many futures and awaits all of them. Something like
```cpp
template<SomeFuture F> // F satisfies Future<T> for some T
void AwaitAll(std::vector<F> futures){
	for(auto& future : futures){
		std::move(future) | futures::Await();
	}
}
```
Objects `first` and `second` have a different type, so they can't be pushed to a vector. Let's fix that with `futures::Box`:
```cpp
futures::BoxedFuture<int> first = futures::Submit(pool, []{
	return result::Ok(42);
}) | futures::Box();

futures::BoxedFuture<int> second = futures::Submit(pool, []{
	fmt::println("I'm different!");
	return result::Ok(37);
}) | futures::Box();
```
Now you can write now
```cpp
vector<futures::BoxedFuture<int>> vec{};
vec.push_back(std::move(first));
vec.push_back(std::move(second));
AwaitAll(std::move(vec));
```
Now we will look at some other futures:
##### `Value`
`futures::Value` represent a value, ready to be taken
```cpp
auto f = futures::Value(42);
auto r = std::move(f) | futures::Await(); // finishes immediatelly
fmt::println("Value {}", *r); // 42
```
There is not `Value<void>` . Instead, we use `Unit = std::monostate`. 

##### `Just`
`futures::Just` is convenient representation of `Value<Unit>`.

##### `Failure`
`futures::Failure` represents an error, ready to be received:
```cpp
auto f = futures::Failure<int>(std::make_error_code(std::errc::timeout_error)); 
// Type must be specified explicitly
auto r = std::move(f) | futures::Await();
assert(!r); // Contains error
```

##### `AndThen`
Now we will look at combinators. You have already seen one -- a combinator `futures::Box`. Another combinator is `futures::AndThen` which applies function to a value delivered by underlying future:
```cpp
auto f = futures::Value(42) | futures::AndThen([](int v){
	return result::Ok(v + 1);
});
auto r = std::move(f) | futures::Await();
fmt::println("Value is {}", *r); // 43
```
If the underlying future returns an error, `AndThen` doesn't execute the lambda and propages the error further.

##### `OrElse`
`futures::OrElse` allows you to recover from errors:
```cpp
auto f = futures::Failure<int>(std::make_error_code(std::errc::timeout_error));
auto ff = std::move(f) | futures::OrElse([](Error){ // Error is alias for error_code
	fmt::println("Recovering from an error");
	return result::Ok(42); // recovery
});

auto r = std::move(ff) | futures::Await();
fmt::println("Recovered value {}", *r);
```

##### `Map`
`futures::Map` is like `AndThen` but lambdas return type should be `T` instead of `Result<T>`:
```cpp
auto f = futures::Value(42) | futures::Map([](int v){
	return v + 1;
});
auto r = std::move(f) | futures::Await();
fmt::println("Value is {}", *r); // 43
```

##### `Flatten`
If your future returns another future, you can used `Flatten` to squash future's type and logic down to `Future<T>` from `Future<Future<T>>`:
```cpp
Future<int> auto f = futures::Submit(pool, [&]{
	return futures::Submit(pool, []{
		return result::Ok(42);
	});
}) | futures::Flatten();

auto r = std::move(f) | futures::Await();
fmt::println("Value is {}", *r); // 42
```

##### `FlatMap`
`futures::FlatMap` if basically `Map` + `Flatten`:
```cpp
auto f = futures::Value(42) | futures::FlatMap([&](int v){
	return futures::Submit(pool, [&]{
		return result::Ok(v + 1);
	});
});
auto r = std::move(f) | futures::Await();
fmt::println("Value is {}", *r); // 43
```

##### Via
By default, every execution is done inline or in the mentioned executor in case of `Submit`. Each following bit of user code (like lambda in `AndThen`) inherits the executor from a previous future:
```cpp
auto f = futures::Submit(pool, []{
	return result::Ok(42); // executed in a pool
}) | futures::AndThen([](int v){
	return result::Ok(v + 1); // also executed in a pool
});
auto r = std::move(f) | futures::Await();
fmt::println("Value is {}", *r); // 43
```
If you want to manually set the executor for the rest of the chain, you can use `futures::Via`:
```cpp
auto f = futures::Value(42) | futures::Via(pool) | futures::AndThen([](int v){
	return result::Ok(v + 1); // executed in a pool
});
auto r = std::move(f) | futures::Await();
fmt::println("Value is {}", *r); // 43
```


### Advanced features




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


