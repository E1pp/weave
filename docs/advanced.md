# Advanced features

## 1. Auto-Boxing
You don't have to use `futures::Box`. Just specify the type instead:
```cpp
futures::BoxedFuture<int> f = futures::Submit(pool, []{
	return result::Ok(42);
});
```

## 2. Function signature compelition

You don't have to write `result::Ok` wrapper anywhere:
```cpp
auto f = futures::Value(42) | futures::AndThen([](int){
	return 42; // Ok. Competes to result::Ok(42);
});
```
You don't have to write return at all:
```cpp
auto f = futures::Value(42) | futures::AndThen([](int){
}); // Completes with return result::Ok();
```
You don't have to write `(InputType)` part as well
```cpp
auto f = futures::Value(42) | futures::AndThen([]{
/*user code*/
});
 // Completes to [](int){/*user code*/}
```
Input type complition can be found too agressive as it allows you to forget to use `[[nodiscard]]` objects. You can set compile flag `WEAVE_AGRESSIVE_AUTOCOMPLETE` to `OFF` in order to make it only work if expected InputType if `Unit` (`void` to `Unit` promotion).

Every object in namespace `futures` which takes invokable as an argument, autocompletes it using the rules above.

## 3. Vector<Future> input
Combinators `First`/`All`/`Quorum` accept `std::vector<Future>` as an argument:
```cpp
std::vector<futures::BoxedFuture<int>> vec{};

for(int i = 0; i < 5; ++i){
	vec.emplace_back(futures::Submit(pool, [i]{
		fmt::println("Hello from replica");
		return i + 1;
	});
}

auto all = futures::All(std::move(vec));

std::move(all) | futures::AndThen([](std::vector<int> ans){
	for(auto& a : ans){
		fmt::println("One of the results {}", a);
	}
}) | futures::Await();
```
`Select` doesn't support vector type.

## 4. `Contract`
`futures::Contract<T>` gives you `Promise<T>` + `EagerFuture`. You can give promise to a producer which can use `Set`/`SetValue`/`SetError` to fulfill it. `EagerFuture` can be used as a normal future which result is ready when promise is fulfilled.
```cpp
auto [f, p] = futures::Contract<int>();

std::move(p).SetValue(42);

auto r = std::move(f) | futures::Await();
fmt::println("Result is {}", *r);
```
Sometimes it is easier to test certains things with `Contract` but you should probably avoid using it as there are safer wait to do whatever you might want to do. You can shoot yourself in the foot using `Contract` quite easily:
```cpp
auto [f, p] = futures::Contract<int>();

auto r = std::move(f) | futures::Await(); // Deadlock
std::move(p).SetValue(42);

fmt::println("Result is {}", *r);
```

## 5. `SleepFor`/`After`
You can make fiber go to sleep without occupying the thread using `fibers::SleepFor`.  You still need timer processor just like in `WithTimeout`
```cpp
futures::Submit(pool, []{
	fibers::SleepFor(5s);
	fmt::println("I've slept");
}) | futures::Detach();
```
You can tell fiber what to do once it wakes up declaratively using `futures::After`:
```cpp
futures::Submit(pool, []{
	futures::After(5s) | futures::AndThen([]{
		fmt::println("I've slept");
	}) | futures::Await();
}) | futures::Detach();
```
Again, you need a time processor in a global scope to write it like in the listing so don't forget to make one and use `MakeGlobal` or `DelayFromThis`.

Another thing to watch out for: while fiber is suspended, ThreadPool doesn't see it so if there is noone else in the pool, `WaitIdle` will return control even though fiber is still asleep. The following code is "How not to use After":
```cpp
// There is a timer processor somewhere before here which exists all the time
{
	executors::ThreadPool pool{4};
	pool.Start();

	futures::Submit(pool, [&]{
		futures::After(5s) | futures::AndThen([]{
			fmt::println("I've slept");
		}) | futures::Await();
	}) | futures::Detach();

	pool.WaitIdle(); // returns control instantly

	// fiber is still asleep

	pool.Stop();
} // <- scope is over, pool destroyed
/* some code*/
// <- fiber is awake and attempts to reschedule itself to the destroyed pool == SEGFAULT
```
Therefore you shouldn't use `After` with `Detach`. You probably shouldn't use `After` and `SleepFor` at all if you don't know what you are doing.

## 6. Cancellation: a brief introduction
Futures in `weave` support cooperative cancellation. This produces some overhead as it requires vtable lookups and threadlocal caching but it is not that bad. The main benefit is short-circuiting:
```cpp
auto slow = futures::Submit(pool, []{
	return 42;
});

auto fast = futures::Value(7);

auto result = futures::First(std::move(slow), std::move(fast)) | futures::Await();
fmt::println("Res is {}", *result): // Always 7
// Almost always the lambda in slow is not even submitted to the pool -- it is just cancelled
```
This allows `WithTimeout` to exist and work as intented:
```cpp
auto never = futures::Submit([]{
	while(true){
		fibers::Yield();
	}
	
	return 13;
});

auto result = std::move(never) | futures::WithTimeout(5s) | futures::Await(); 
// <- After 5s
assert(!result); // <- std::errc::timed_out
```
What supports cancellation:
1. Every future in `futures`
2. Timers (there isn't much of them right now, but `StandaloneProcessor` is smart enough). Check [unit tests](tests/timers/futures) to see how much it can do.
3. `fibers::Yield` and `fibers::SleepFor`

## 7. Cancellation: `RequestCancel`
`ContractFuture` and `StartFuture` have `RequestCancel` method, which can manually cancel the underlying evaluation:
```cpp
auto f = futures::Submit(pool, []{}) | futures::AndThen([]{}) | futures::Start();
std::move(f).RequestCancel(); // If we are fast enough, zero tasks will be submitted
// to the pool
```
## 8. Cancellation: Inheritance
You can cancel the future inside another future:
```cpp
auto parent = futures::Submit(pool, [&]{
	
	auto child = futures::Submit(pool, []{
		while(true){
			fiber::Yield();
		}
	}) | futures::Start();
	
	std::move(child ) | futures::Await(); // Successfully cancels from cancellation of parent 
}) | futures::Start();

std::move(parent ).RequestCancel();
```

## 9. Add a side effect
You can use combinators `OnCancel`/`OnSuccess`/`Anyway` to execute a lambda based on whether the underlying future was cancelled or not:
```cpp
auto f = futures::Submit(pool, []{}) | futures::OnCancel([]{
	fmt::println("Cancelled");
}) | futures::Start();

std::move(f).RequestCancel(); // prints "Cancelled"
```
`OnSuccess` and `Anyway` do the same thing but on different self-explanatory condition.

It is important to note, that inheritance from the previous section doesn't work for lambdas in side-effects. If you run something in `OnCancel` or any other, user code inside will assume that noone is trying to cancel its execution. This means, that `OnCancel` will finish working only when the given lambda returns control. You cannot timeout `OnCancel` execution from the outside of lambda's scope:
```cpp
auto f = futures::Submit(pool, {}) | futures::Anyway([]{
	while(true){
		fibers::Yield(); // will never stop even though we cancelled the f
	}
}) | futures::Start();

std::move(f).RequestCancel();
```

## 10. A quick detour: traits.
`weave` supports the following traits: `Eager`, `Lazy`, `Cancellable`, all of which apply to futures. There is also `JustCancellable` which drops requirement of being a future. 

Every future apart from `BoxedFuture<T>` is `Cancellable`. `StartFuture` and `ContractFuture` are `Eager`. You can use these traits to create template specializations. 

If you want to use type erasure of `BoxedFuture` but have `Cancellable` trait, use `CBoxedFuture` and `CBox` instead.

## 11. Cancellation: `no_alloc` overloads
Every parallel combinator has a `no_alloc` version in the same include file. These methods have the same signature but require every future to be `Cancellable`. You can use them just like this:
```cpp
auto f = futures::no_alloc::First(futures::Value(42), futures::Value(37));
auto r = std::move(f) | futures::Await(); // no dynamic memory allocations in this code
```
`no_alloc` algorithms allow parallel combinators to not allocate memory for shared state at the cost of more complicated semantics: now `First` finishes only when it either gets the last error or when it gets first value and every other future cancels or finishes their execution. 
```cpp
auto [f1, p] = futures::Contract<int>();

auto f = std::move(f1) | futures::OnCancel([]{
	fmt::println("Wait for me!");
});

futures::no_alloc::First(futures::Value(42), std::move(f)) | futures::AndThen([]{
	fmt::println("First done!");
}) | futures::Detach();

// Nothing will be printed untill we fulfill the other future
std::move(p).SetValue(37); // Prints "Wait for me!" and then "First done"
```
If you futures are cancellable and you don't do (and you shouldn't do) anything heavy in the cancellation path, differences between normal and `no_alloc` semantic will be irrelevant to you. If that is so, you should use `no_alloc` exclusively as it is more optimal, and also helps to partially alleviate one problem with the current cancellation model.

## 12. `[[nodiscard]]` and Cancellation
Every future is marked `[[nodiscard]]` and you mustn't discard them. Technically, function signature compelition allow you to cheat this rule, but there is also a problem with cancellation:
```cpp
auto f = futures::Submit(pool, []{}) | futures::Start(); // memory is allocated by Start

auto g = futures::Submit(pool, []{}) | futures::AndThen([f = std::move(f)]{
	std::move(f) | futures::Await(); // memory is deallocated here
}) | futures::Start();

std::move(g).RequestCancel();
```
Another example would be first `Submit` in `g` returning an error, causing `AndThen` to not run its lambda. In either cases `f` will technically be discarded and you might be wondering if this is a memory leak scenario. I'm happy to inform you that discarding any kind of future and promise are accounted for and you don't have to worry about it. However, if you have your own `[[nodiscard]]` objects which you move into lambda's fields, you might want to worry, since they can, in fact, be discarded.

## 13. Cancellation: design flaw.
Now, let's talk a little bit about implementation. Every consumer has a method `Cancel` which allows producer to report that it was cancelled to the consumer. Consumer also has a method `CancelToken` which returns a special object `cancel::Token` which is basically a handle to the interface of `SignalSender` -- abstraction, which can send a cancellation signal or a release signal. First one implies that cancellation was requested, second one -- there will be no cancellation request ever. There is also a `SignalReceiver` abstraction which subscribe to cancellation so that whenever `SignalSender` sends a signal to the subscribed `SignalReceiver`, it run `SignalReceiver`'s callback, namely `Forward` to properly propagate the cancellation.

Now, this model has one major implication: in order for lifetimes to be syncronized, `SignalSender` must hold a strong reference to the `SignalReceiver` -- otherwise, receiver might deallocate it's resources right before sender decides to use `Forward`. This means, that subscribing to `SignalSender` increases strong references count and `Forward` reduces it. In order to optimally deallocate resources, you will want to be able to unsubscribe from cancellation when you are done -- forcefully retract the strong reference you have given. 

So where is the problem? Parallel combinators use the same implementation of `SignalSender` called `JoinSource` which places it's `SignalReceiver` into a lock-free queue. You cannot remove yourself from this queue, meaning, you cannot retract your reference until that `SignalSender` releases it on its own. Let's look at the code:
```cpp
auto f = futures::Submit([]{
	for(int i = 0; i < 100; ++i){
		futures::Just() | futures::Start() | futures::Await();
	}
	return 42;
});

auto [f1, p1] = futures::Contract<int>();

futures::First(std::move(f), std::move(f1)) | futures::Detach();

std::move(p1).SetValue(37);
```
Future `f` allocates memory for `Start` 100 times but it doesn't release it until the `First` future is done. This means that until we fulfill the promise `p1`, memory will be left unused.

Now, instead of `Start` there could be the `First` future itself as it also subscribes to the cancellation. If you use a normal version, it would allocate the memory for the shared state and hold it just like `Start`. If you use `no_alloc` version, there is no allocation for shared state and therefore it is not withheld by outer `First`.

**TL;DR**
If you use parallel combinators, don't spam too much memory allocations inside of futures you send to these combinators as this memory will be released only eventually.