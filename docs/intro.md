# Getting Started

This library has a lot of Unit [tests](tests) which will probably explain how everything works much better than I ever would, so you should really consider reading them. Below are the most important things you should know about `weave`.

## 1. First  look at `executors`
Executor is an abstraction which launches user's code. We have a function `executors::Submit` which can be used to send lambdas to any executor. 

### `ThreadPool`
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

### `ManualExecutor`
`ManualExecutor` can be used to run tasks manually which can prove helpful in deterministic testing
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
 
## 2. "Forget about `executors::Submit`" or First look at `futures`
Futures are objects which represent a value which might not be ready at the moment, and the underlying computation which would return this value. Futures in `weave` are lazy, meaning that they will postpone any kind of evaluations until they know how exactly their value will be used. This model allows us to avoid numerous dynamic allocations and vtable lookups since we always know (almost) every consumer at compile-time. Now, let's look how we can come up with a better API for submitting lambdas.

### `Submit`
Using `executors::Submit` is very simple but inefficient. It uses type erasure which can be avoided. Submitted task can be represented as future, also known as `futures::Submit`:
```cpp
executors::ThreadPool pool{/*threads=*/4};

pool.Start();

auto f = futures::Submit(pool, []{
	fmt::println("Running on a ThreadPool!");
}); // Task is not yet submitted! No allocations were done
...
```

Next step in our replacement of `executors::Submit` would be actually getting task to run. We can do it in several ways:

### `Detach`
We can let the task run on its own without getting the return value using `futures::Detach`:
```cpp
executors::ThreadPool pool{/*threads=*/4};

pool.Start();

auto f = futures::Submit(pool, []{
	fmt::println("Running on a ThreadPool!");
});

std::move(f) | futures::Detach(); // Future is moved on heap and lambda is submitted

pool.WaitIdle();

pool.Stop();
```
But wait! We are still allocating the memory, so what's the point of this? Indeed, doing `Detach` on `Submit` does exactly the same thing that `executors::Submit` does. However, we can optimize that using the fact that we are synchronously waiting for task to complete, which leads us to

###  `Await`

We can synchronously wait for task to be executed using `futures::Await`:
```cpp
executors::ThreadPool pool{/*threads=*/4};

pool.Start(); 

auto f = futures::Submit(pool, []{
	fmt::println("Running on a ThreadPool!");
	return result::Ok(42); // ???
}); 

auto result = std::move(f) | futures::Await();
// lambda was already executed at this point
pool.Stop();
```
In this example there are zero memory allocations and exactly two vtable lookups: who is the `IExecutor` and who is the `Task` (Technically, there are few more lookups related to the internal structure of a thread pool but should we use a simplier executor, the number would be exactly two).  

Now you might be wondering what types `f` and `result` are, which we are going to discuss now.

### `Result<T>` and `Future<T>`
The example above can be rewritten using type `Result<T>` and concept `Future<T>` like this
```cpp
executors::ThreadPool pool{/*threads=*/4};

pool.Start(); 

Future<int> auto f = futures::Submit(pool, []{
	fmt::println("Running on a ThreadPool!");
	return result::Ok(42);
}); 

Result<int> result = std::move(f) | futures::Await();

pool.Stop();
```
`Result<T>` represents an object which is either the value itself or an error -- just in case the evaluation aborts for some reason. The underlying type is `tl::expected<T, std::error_code>` which allows you to use `expected` library API on these objects. `weave` provides convenient functions `result::Ok` and `result::Err` for return types in futures. 

We use concept `Future<T>` instead of concrete type because we store a lot of information in future's type using some template metaprogramming magic. Now, this can be very incovenient if you are writing an interface which accepts the future or you simply want to store a bunch of them in an STL-container. In this case you can use

### `Box`
`Box` allows you to erase future's type. Suppose you have two futures:
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
Objects `first` and `second` have a different type, so they can't be pushed into a vector. Let's fix that with `futures::Box`:
```cpp
futures::BoxedFuture<int> first = futures::Submit(pool, []{
	return result::Ok(42);
}) | futures::Box();

futures::BoxedFuture<int> second = futures::Submit(pool, []{
	fmt::println("I'm different!");
	return result::Ok(37);
}) | futures::Box();
```
Now you can write
```cpp
vector<futures::BoxedFuture<int>> vec{};
vec.push_back(std::move(first));
vec.push_back(std::move(second));
AwaitAll(std::move(vec));
```

### `Value`
`futures::Value` represent a value, ready to be used
```cpp
auto f = futures::Value(42);
auto r = std::move(f) | futures::Await(); // finishes immediatelly
fmt::println("Value {}", *r); // 42
```
There is no `Value<void>` . Instead, we use `Unit = std::monostate`. 

### `Just`
`futures::Just` is a convenient representation of `Value<Unit>`.

### `Failure`
`futures::Failure` represents an error, ready to be received:
```cpp
auto f = futures::Failure<int>(std::make_error_code(std::errc::timed_out)); 
// Type must be specified explicitly
auto r = std::move(f) | futures::Await();
assert(!r); // Contains error
```

## 3. Second look at `futures`. Sequential combinators.
Now we will look at combinators. You have already seen one -- `futures::Box`. As you have already noticed, they are applied like this
```cpp
std::move(futures) | CombinatorObject;
```

### `AndThen`
Another combinator is `futures::AndThen` which applies function to a value delivered by underlying future:
```cpp
auto f = futures::Value(42) | futures::AndThen([](int v){
	return result::Ok(v + 1);
});
auto r = std::move(f) | futures::Await();
fmt::println("Value is {}", *r); // 43
```
If the underlying future returns an error, `AndThen` doesn't execute the lambda and propages the error further.

### `OrElse`
`futures::OrElse` allows you to recover from errors:
```cpp
auto f = futures::Failure<int>(std::make_error_code(std::errc::timed_out));
auto ff = std::move(f) | futures::OrElse([](Error){ // Error is an alias for error_code
	fmt::println("Recovering from an error");
	return result::Ok(42);
});

auto r = std::move(ff) | futures::Await();
fmt::println("Recovered value {}", *r);
```

### `Map`
`futures::Map` is like `AndThen` but lambdas return type should be `T` instead of `Result<T>`:
```cpp
auto f = futures::Value(42) | futures::Map([](int v){
	return v + 1;
});
auto r = std::move(f) | futures::Await();
fmt::println("Value is {}", *r); // 43
```

### `Flatten`
If your future returns another future, you can use `Flatten` to squash future's type and logic down to `Future<T>` from `Future<Future<T>>`:
```cpp
Future<int> auto f = futures::Submit(pool, [&]{
	return futures::Submit(pool, []{
		return result::Ok(42);
	});
}) | futures::Flatten();

auto r = std::move(f) | futures::Await();
fmt::println("Value is {}", *r); // 42
```

### `FlatMap`
`futures::FlatMap` is basically `Map` + `Flatten`:
```cpp
auto f = futures::Value(42) | futures::FlatMap([&](int v){
	return futures::Submit(pool, [&]{
		return result::Ok(v + 1);
	});
});
auto r = std::move(f) | futures::Await();
fmt::println("Value is {}", *r); // 43
```

### Via
By default, every execution is done inline or in a mentioned executor in case of `Submit`. Each following bit of user code (like lambda in `AndThen`) inherits the executor from a previous future:
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
Using another `Via` updates the executor to a new one.

### `Start`
If you want to begin evaluation of the future before you know the consumer, you can use `futures::Start`:
```cpp
auto eager = futures::Submit(pool, []{
	fmt::println("Running on pool!");
	return result::Ok(42);
}) | futures::Start(); // <- Task is submitted to the pool already

auto lazy = std::move(eager) | futures::AndThen([](int){
	return result::Ok("Another result"); 
	// <- This will not be evaluated right away
	// future becomes lazy again
}); 
```
`Start` uses a heap allocation.

### `Fork`
If you want to take future's result to several combinators, you can use `futures::Fork<N>`:
```cpp
auto initial = futures::Submit(pool, []{
	return result::Ok(42);
}); // Only one task is submitted to the pool

auto [left, right] = std::move(initial) | futures::Fork<2>();

auto r1 = std::move(left) | futures::Await();
auto r2 = std::move(right) | futures::AndThen([](int v){
	return v + 1;
}) | futures::Await();

fmt::println("r1->{} r2->{}", *r1, *r2); // 42, 43
```

## 4. Third look at `futures`. Parallel composition

### `First`
If you want to wait for the first future, you can use `futures::First`:
```cpp
auto f1 = futures::Submit(pool, []() -> Result<int>{
	return result::Err(std::make_error_code(std::errc::timed_out));
});
auto f2 = futures::Submit(pool, []{
	return result::Ok(37);
});

auto first = futures::First(std::move(f1), std::move(f2));

auto res = std::move(first) | futures::Await();

fmt::println("res->{}", *res); // 37
```
`futures::First` contains the value of the first successful future or the error of the last failing future if all of them have failed.

### `All`
`futures::All` contains value of every future or the error of the first failed one.

### `Quorum`
`futures::Quorum` contains the values of the first `threshold`  successful futures or the error of the first failed future after which it is impossible to contain `threshold` values.

### `Select`
`futures::Select` is just like `futures::First` but contains `std::variant` instead of plain `T`, but what is more important, it records the first future which was finished even if its result contains an error.

## 5. First look at `fibers`
`weave` has stackfull fibers which are just like golangs coroutines. They can be cooperatively suspended without occupying the thread and provide an extensive library of sync primitives. In order to use fibers' API and sync primitives lambda has to be executed in fiber's context.

### "How to run lambda in fiber's context?"  or Second look at `executors`
`executors::fibers::ThreadPool` and `executors::fibers::ManualExecutor` got you covered:
```cpp
executors::fibers::ThreadPool pool{4};
pool.Start();

futures::Submit(pool, []{
	// Running in fiber's context
}) | futures::Await();

pool.Stop();
```
```cpp
executors::fibers::ManualExecutor manual;

futures::Submit(manual, []{
	// Running in fiber's context
}) | futures::Detach();

manual.RunNext();

manual.Stop(); // fibers manual needs to be stopped!
```

`executors::ThreadPool` is an alias for `executors::fibers::ThreadPool`, so fiber pool is the default implementation.

### `Yield`
`fibers::Yield` suspends fiber and reschedules it as the very last task to be executed. 
```cpp
executors::fibers::ManualExecutor manual;

futures::Submit(manual, []{
	fmt::println("Step 1");
	fibers::Yield();
	fmt::println("Step 3");
}) | futures::Detach();

futures::Submit(manual, []{
	fmt::println("Step 2");
	fibers::Yield();
	fmt::println("Step 4");
}) | futures::Detach();

manual.Drain();
// Prints
// Step 1 -> Step 2 -> Step 3 -> Step 4

manual.Stop(); // fibers manual needs to be stopped!
```

### `Event`
`fibers::Event` object provides contract: `Event::Wait` + `Event::Fire`: `Wait` suspends fibers until `Fire` is called:
```cpp
executors::fibers::ThreadPool pool{4};
pool.Start();

bool flag = false;
fibers::Event event;

futures::Submit(pool, [&]{
	fmt::println("Step 1");
	event.Wait();
	fmt::println("Step 2");
	assert(flag);
}) | futures::Detach();

futures::Submit(pool, [&]{
	flag = true;
	fmt::println("This message appears before Step 2!");
	event.Fire();
}) | futures::Detach();

pool.WaitIdle();

pool.Stop();
```

### `Mutex`
`fibers::Mutex` is a mutex for fibers:
```cpp
executors::fibers::ThreadPool pool{4};
pool.Start();

int non_atomic = 0;
fibers::Mutex mutex;

for(int i = 0; i < 5; ++i){
	futures::Submit(pool, [&]{
		std::lock_guard guard(mutex);
		non_atomic++;
	}) | futures::Detach();
}

fmt::println("Counter {}", non_atomic); // 5

pool.WaitIdle();

pool.Stop();
```
### `WaitGroup`
`fibers::WaitGroup` is a wait group from [golang](https://gobyexample.com/waitgroups).

### `Channel`
`fibers::Channel`  is just like a channel from [golang](https://gobyexample.com/channels). API example:
```cpp
fibers::Channel<int> msgs{16};

// Producer
// Capture channel by value
futures::Submit(pool, [msgs]() mutable {
	for (int i = 0; i < 128; ++i) {
		msgs.Send(i);
	}
	
	// Poison pill
	msgs.Send(-1);
});

// Consumer
while (true) {
	int value = msgs.Receive();
	
	if (value == -1) {
		break;
	}
	
	fmt::println("Received value {}", value);
}
```

### `Select`/`TrySelect`
`fibers::Select` is also just like select from [golang](https://gobyexample.com/select). Let's look at it's API
```cpp
fibers::Channel<int> xs;
fibers::Channel<int> ys;

// value - std::variant<int, int>
auto value = fibers::Select(Receive{xs}, Receive{ys});
switch (value.index()) {
   case 0:
     // Handle std::get<0>(value);
     break;
  case 1:
     // Handle std::get<1>(value);
     break;
``` 
```cpp
fibers::Channel<int> xs;
fibers::Channel<int> ys;

// value - std::variant<int, Unit>
auto value = fibers::Select(Receive{xs}, Send{ys, 5});
switch (value.index()) {
   case 0:
     // Handle std::get<0>(value);
     break;
  case 1:
     // Handle std::get<1>(value);
     break;
```
### `Await`
You can use `futures::Await` to suspend fiber until the future is ready. You can use it in fibers context just like in a normal one without blocking the thread.

## 6. Timers
In order to use timers you need a timer processor. At the moment, `weave` has only one processor, `StandaloneProcessor`. Let's look at the combinator `WithTimeout`, which is self-explanatory, to see how to use processors:
```cpp
timers::StandaloneProcessor proc{};

executors::ThreadPool pool{4}; // We don't start pool so it won't run any tasks

auto res = futures::Submit(pool, []{
	return result::Ok(42); // Will never run
}) | futures::WithTimout(proc.DelayFromThis(1s)) | futures::Await();

// Will be released after 1 second and will return a TimeoutError 

assert(!res);

pool.Start();
pool.Stop();
```
Using `DelayFromThis` can be a bit too verbose, so you can store pointer to your processor in the global scope using `MakeGlobal`. Just don't forget that this operation doesn't extend the lifetime of processor, so you mustn't forget to reset it using `satellite::ResetGlobalProcessor`.
```cpp
timers::StandaloneProcessor proc{};
proc.MakeGlobal();

executors::ThreadPool pool{4}; // We don't start pool so it won't run any tasks

auto res = futures::Submit(pool, []{
	return result::Ok(42); // Will never run
}) | futures::WithTimout(1s) | futures::Await();

// Will be released after 1 second and will return a TimeoutError 

assert(!res);

pool.Start();
pool.Stop();
satellite::ResetGlobalProcessor();
```

## 7. Cancellation: what to be aware of
Cancellation is discussed in detail in "Advanced features". Here are some things you should keep in mind:

### Some error codes are reserved:

1.`WithTimeout` can return `std::errc::timed_out`
2. Sometimes you can read `std::errc::operation_canceled` from `Contract` (which you shouldn't use by the way). "What `Contract` is" is discussed in "Advanced features"

### Some functions may throw
Exceptions are used as an unwinding mechanism for cancellation. This means that you **have** to use RAII wherever it is relevant. Below are the functions which can throw:
1. `futures::Await`
2. `fibers::Yield`
3. `fibers::SleepFor`

You shouldn't interfere with exceptions they throw as this process is part of cancellation of futures. Again, if you want to learn more about cancellation, go to "Advanced features".
