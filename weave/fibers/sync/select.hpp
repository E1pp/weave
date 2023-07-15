#pragma once

#include <weave/fibers/sched/suspend.hpp>

#include <weave/fibers/sync/detail/meta.hpp>

#include <weave/fibers/sync/channel.hpp>
#include <weave/fibers/sync/waiters.hpp>

#include <weave/support/fast_rand.hpp>

#include <weave/threads/lockfree/rendezvous.hpp>
#include <weave/threads/blocking/stdlike/mutex.hpp>

#include <twist/ed/local/val.hpp>
#include <twist/ed/stdlike/random.hpp>

#include <chrono>
#include <mutex>  // scoped lock
#include <numeric>
#include <optional>
#include <random>
#include <variant>

// https://golang.org/src/runtime/chan.go
// https://golang.org/src/runtime/select.go

namespace weave::fibers {

namespace detail {

///////////////////////////////////////////////////////////////////////////////

twist::ed::ThreadLocal<support::FastRand> rand{
    support::FastRand{static_cast<uint64_t>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count())}};

template <size_t Size>
std::array<size_t, Size> IterationStrategy() {
  size_t first = (*rand)() % Size;
  std::array<size_t, Size> indices;
  std::iota(indices.begin(), indices.end(), 0);

  std::rotate(indices.begin(), indices.begin() + 1,
              indices.begin() + first + 1);

  return std::move(indices);
}

///////////////////////////////////////////////////////////////////////////////

template <bool TryVersion, size_t Index, SelectorAlternative T,
          SelectorAlternative... Types>
struct SelectorLeaf;

template <bool TryVersion, typename T, SelectorAlternative... Types>
struct SelectorBase;

template <bool TryVersion, SelectorAlternative... Alts>
class Selector
    : public SelectorBase<TryVersion, std::make_index_sequence<sizeof...(Alts)>,
                          Alts...> {
  enum OverloadType : size_t { Suspending = 0, NonSuspending = 1 };

  template <bool Flag, size_t Index, SelectorAlternative T,
            SelectorAlternative... Ts>
  friend struct SelectorLeaf;

  template <typename T>
  friend class ChannelImpl;

  using SelectedValue = SelectedValue<Alts...>;
  using MaybeSelectedValue = MaybeSelectedValue<Alts...>;

 public:
  auto Select(Alts... alts) {
    auto awaiter = [this, &alts...](FiberHandle handle) {
      fiber_ = handle;

      // pack into tuple for runtime iteration
      std::tuple<Alts...> tuple_chan = std::make_tuple(std::move(alts)...);

      for (auto index : IterationStrategy<sizeof...(Alts)>()) {
        TupleVisit<OverloadType::Suspending>(tuple_chan, index);
      }

      // rendezvous with successful producer

      if (bool both = rendezvous_.Consume()) {
        return fiber_;
      }

      return FiberHandle::Invalid();
    };

    Suspend(awaiter);

    // we have the value
    WHEELS_VERIFY(variant_storage_.index() == 1,
                  "Empty storage after suspension!");

    // we have to remove ourselves from every channel queue:
    RemoveFromQueues(alts...);

    return std::move(std::get<1>(variant_storage_));
  }

  auto TrySelect(Alts... alts) {
    // pack into tuple for runtime iteration
    std::tuple<Alts...> tuple_chan = std::make_tuple(std::move(alts)...);

    {
      std::scoped_lock scoped(alts.chan_->impl_->chan_spinlock_...);

      for (auto index : IterationStrategy<sizeof...(Alts)>()) {
        TupleVisit<OverloadType::NonSuspending>(tuple_chan, index);
      }
    }

    if (variant_storage_.index() == 0) {
      return MaybeSelectedValue{std::in_place_index<sizeof...(Alts)>,
                                std::monostate{}};
    }

    return std::move(std::get<2>(variant_storage_));
  }

  /////////////////////////////////////////////////////////////////

  // QUEUE REMOVAL

  /////////////////////////////////////////////////////////////////

 private:
  void RemoveFromQueues(Alts&... alts) {
    // Unlink goes there

    [&]<size_t... Inds>(std::index_sequence<Inds...>) {
      (UnlinkOne<Inds>(alts), ...);
    }
    (std::index_sequence_for<Alts...>());
  };

  template <size_t Index>
  void UnlinkOne(NthType<Index, Alts...>& alternative) {
    using Alternative = NthType<Index, Alts...>;
    using Type = typename Alternative::Type;

    auto* leaf =
        static_cast<SelectorLeaf<TryVersion, Index, Alternative, Alts...>*>(
            this);
    Channel<Type>* chan = alternative.chan_;

    threads::blocking::stdlike::LockGuard lock(chan->impl_->chan_spinlock_);
    leaf->Unlink();
  }

  /////////////////////////////////////////////////////////////////

  // Channel Registration

  /////////////////////////////////////////////////////////////////

  template <size_t Index>
  void SuspendingRegistration(
      Receive<typename NthType<Index, Alts...>::Type>& alt) {
    auto* leaf = static_cast<
        SelectorLeaf<TryVersion, Index, NthType<Index, Alts...>, Alts...>*>(
        this);
    Channel<typename NthType<Index, Alts...>::Type>* chan = alt.chan_;

    threads::blocking::stdlike::LockGuard lock(chan->impl_->chan_spinlock_);

    if (chan->impl_->TryCompleteReceiver(leaf) == RendezvousResult::Success) {
      rendezvous_.Produce();
      return;
    }

    chan->impl_->queue_.PushBack(leaf);
  }

  template <size_t Index>
  void SuspendingRegistration(
      Send<typename NthType<Index, Alts...>::Type>& alt) {
    auto* leaf = static_cast<
        SelectorLeaf<TryVersion, Index, NthType<Index, Alts...>, Alts...>*>(
        this);
    Channel<typename NthType<Index, Alts...>::Type>* chan = alt.chan_;

    leaf->storage_.emplace(std::move(alt.storage_));

    threads::blocking::stdlike::LockGuard lock(chan->impl_->chan_spinlock_);

    if (chan->impl_->TryCompleteSender(leaf) == RendezvousResult::Success) {
      rendezvous_.Produce();
      return;
    }

    chan->impl_->queue_.PushBack(leaf);
  }

  /////////////////////////////////////////////////////////////////

  template <size_t Index>
  void NonSuspendingRegistration(
      Receive<typename NthType<Index, Alts...>::Type>& alt) {
    auto* leaf = static_cast<
        SelectorLeaf<TryVersion, Index, NthType<Index, Alts...>, Alts...>*>(
        this);
    Channel<typename NthType<Index, Alts...>::Type>* chan = alt.chan_;

    chan->impl_->TryCompleteReceiver(leaf);
  }

  template <size_t Index>
  void NonSuspendingRegistration(
      Send<typename NthType<Index, Alts...>::Type>& alt) {
    using ValueType = typename NthType<Index, Alts...>::Type;
    using LeafType =
        SelectorLeaf<TryVersion, Index, NthType<Index, Alts...>, Alts...>;

    [[maybe_unused]] auto* leaf = static_cast<LeafType*>(this);
    [[maybe_unused]] Channel<ValueType>* chan = alt.chan_;

    // Optimisation allowing sending leaf in TryVersion == true to not have an
    // std::optional field
    struct TemporaryWaiter : ICargoWaiter<ValueType> {
      TemporaryWaiter(LeafType* leaf, ValueType val)
          : leaf_(leaf),
            val_(std::move(val)) {
      }

      void SetHandle(FiberHandle) override {
        std::abort();
      }

      void Schedule(executors::SchedulerHint) override {
        std::abort();
      }

      void WriteValue(ValueType) override {
        std::abort();
      }

      ValueType ReadValue() override {
        MaybeSelectedValue local{std::in_place_index<Index>, std::monostate{}};

        typename LeafType::SelectorType* selector =
            static_cast<typename LeafType::SelectorType*>(leaf_);

        selector->variant_storage_.template emplace<2>(std::move(local));

        return std::move(val_);
      }

      State MarkUsed() override {
        return leaf_->MarkUsed();
      }

      ~TemporaryWaiter() override = default;

      LeafType* leaf_;
      ValueType val_;
    } tmp_waiter{leaf, std::move(alt.storage_)};

    chan->impl_->TryCompleteSender(&tmp_waiter);
  }

  /////////////////////////////////////////////////////////////////

  // RUNTIME TUPLE ACCESS

  /////////////////////////////////////////////////////////////////

 private:
  template <size_t Index, OverloadType Overload>
  struct VisitImpl {
    template <typename T>
    static void Visit(T&, size_t, Selector<TryVersion, Alts...>*) {
      std::abort();
    }
  };

  template <size_t Index>
  struct VisitImpl<Index, OverloadType::Suspending> {
    template <typename T>
    static void Visit(T& tup, size_t ind, Selector<TryVersion, Alts...>* sel) {
      if (ind == Index - 1) {
        sel->SuspendingRegistration<Index - 1>(std::get<Index - 1>(tup));
      } else {
        VisitImpl<Index - 1, OverloadType::Suspending>::Visit(tup, ind, sel);
      }
    }
  };

  template <size_t Index>
  struct VisitImpl<Index, OverloadType::NonSuspending> {
    template <typename T>
    static void Visit(T& tup, size_t ind, Selector<TryVersion, Alts...>* sel) {
      if (ind == Index - 1) {
        sel->NonSuspendingRegistration<Index - 1>(std::get<Index - 1>(tup));
      } else {
        VisitImpl<Index - 1, OverloadType::NonSuspending>::Visit(tup, ind, sel);
      }
    }
  };

  template <>
  struct VisitImpl<0, OverloadType::Suspending> {
    template <typename T>
    static void Visit(T&, size_t, Selector<TryVersion, Alts...>*) {
      std::abort();
    }
  };

  template <>
  struct VisitImpl<0, OverloadType::NonSuspending> {
    template <typename T>
    static void Visit(T&, size_t, Selector<TryVersion, Alts...>*) {
      std::abort();
    }
  };

  template <OverloadType Overload>
  void TupleVisit(std::tuple<Alts...>& tuple_chan, size_t ind) {
    VisitImpl<sizeof...(Alts), Overload>::Visit(tuple_chan, ind, this);
  }

 private:
  // the only possible race is with constructor
  // but hb with constructor is ensured via chan's
  // mutexes lock-unlock hb
  threads::lockfree::RendezvousStateMachine rendezvous_;
  twist::ed::stdlike::atomic<State> state_{State::Ready};

  std::variant<std::monostate, SelectedValue, MaybeSelectedValue>
      variant_storage_;
  FiberHandle fiber_{FiberHandle::Invalid()};
};

template <bool TryVersion, typename Seq, SelectorAlternative... Types>
struct SelectorBase {};

template <bool TryVersion, size_t... Index, SelectorAlternative... Types>
struct SelectorBase<TryVersion, std::index_sequence<Index...>, Types...>
    : public SelectorLeaf<TryVersion, Index, Types, Types...>... {};

template <bool TryVersion, size_t Index, SelectorAlternative T,
          SelectorAlternative... Alts>
struct SelectorLeaf {};

// Receive-Op version of SelectorLeaf. TryVersion is irrelevant here
template <bool TryVersion, size_t Index, typename T,
          SelectorAlternative... Alts>
struct SelectorLeaf<TryVersion, Index, Receive<T>, Alts...>
    : public ICargoWaiter<T> {
  using SelectorType = Selector<TryVersion, Alts...>;

  void SetHandle(FiberHandle) override {
    std::abort();  // handle is set by selector
  }

  void Schedule(executors::SchedulerHint) override {
    auto* selector = static_cast<SelectorType*>(this);

    if (bool both = selector->rendezvous_.Produce()) {
      selector->fiber_.Schedule(executors::SchedulerHint::Next);
    }
  }

  void WriteValue(T val) override {
    SelectorType* selector = static_cast<SelectorType*>(this);

    if constexpr (TryVersion) {
      typename SelectorType::MaybeSelectedValue local{
          std::in_place_index<Index>, std::move(val)};

      selector->variant_storage_.template emplace<2>(std::move(local));
      return;
    }

    typename SelectorType::SelectedValue local{std::in_place_index<Index>,
                                               std::move(val)};
    selector->variant_storage_.template emplace<1>(std::move(local));
  }

  // Never used
  T ReadValue() override {
    std::optional<T> pass{};
    std::abort();
    return std::move(*pass);
  }

  State MarkUsed() override {
    SelectorType* selector = static_cast<SelectorType*>(this);

    return selector->state_.exchange(State::Used, std::memory_order::relaxed);
  }

  ~SelectorLeaf() override = default;
};

// Try version of Sending SelectorLeaf. Only method used is MarkUsed
template <size_t Index, typename T, SelectorAlternative... Alts>
struct SelectorLeaf<true, Index, Send<T>, Alts...> : public ICargoWaiter<T> {
  using SelectorType = Selector<true, Alts...>;

  void SetHandle(FiberHandle) override {
    std::abort();
  }

  void Schedule(executors::SchedulerHint) override {
    std::abort();
  }

  void WriteValue(T) override {
    std::abort();
  }

  // Not Implemented
  T ReadValue() override {
    std::optional<T> pass{};
    std::abort();
    return std::move(*pass);
  }

  State MarkUsed() override {
    SelectorType* selector = static_cast<SelectorType*>(this);

    return selector->state_.exchange(State::Used, std::memory_order::relaxed);
  }

  ~SelectorLeaf() override = default;
};

template <size_t Index, typename T, SelectorAlternative... Alts>
struct SelectorLeaf<false, Index, Send<T>, Alts...> : public ICargoWaiter<T> {
  using SelectorType = Selector<false, Alts...>;

  void SetHandle(FiberHandle) override {
    std::abort();  // handle is set by selector
  }

  void Schedule(executors::SchedulerHint) override {
    auto* selector = static_cast<SelectorType*>(this);

    if (bool both = selector->rendezvous_.Produce()) {
      selector->fiber_.Schedule(executors::SchedulerHint::Next);
    }
  }

  // Never used
  void WriteValue(T) override {
    std::abort();
  }

  T ReadValue() override {
    SelectorType* selector = static_cast<SelectorType*>(this);

    // Set corrent result
    typename SelectorType::SelectedValue local{std::in_place_index<Index>,
                                               std::monostate{}};
    selector->variant_storage_.template emplace<1>(std::move(local));

    // return value from storage
    return std::move(*storage_);
  }

  State MarkUsed() override {
    SelectorType* selector = static_cast<SelectorType*>(this);

    return selector->state_.exchange(State::Used, std::memory_order::relaxed);
  }

  ~SelectorLeaf() override = default;

 public:
  std::optional<T> storage_{};
};

///////////////////////////////////////////////////////////////////////////////

}  // namespace detail

/*
 * Usage:
 * Channel<X> xs;
 * Channel<Y> ys;
 * ...
 * // value - std::variant<X, Y>
 * auto value = Select(Receive{xs}, Receive{ys});
 * switch (value.index()) {
 *   case 0:
 *     // Handle std::get<0>(value);
 *     break;
 *   case 1:
 *     // Handle std::get<1>(value);
 *     break;
 * }
 */

template <SelectorAlternative... Alts>
auto Select(Alts... alts) {
  detail::Selector<false, Alts...> selector{};
  return selector.Select(std::move(alts)...);
}

/*
 * Usage:
 * Channel<X> xs;
 * Channel<Y> ys;
 * ...
 * // value - std::variant<X, Y, std::monostate>
 * auto value = TrySelect(Receive{xs}, Receive{ys});
 * switch (value.index()) {
 *   case 0:
 *     // Handle std::get<0>(value);
 *     break;
 *   case 1:
 *     // Handle std::get<1>(value);
 *     break;
 *   default:
 *     // No value
 *     break;
 * }
 */

template <SelectorAlternative... Alts>
auto TrySelect(Alts... alts) {
  detail::Selector<true, Alts...> selector{};
  return selector.TrySelect(std::move(alts)...);
}

// Backwards compatibility
template <typename... Types>
auto Select(Channel<Types>&... chans) {
  detail::Selector<false, Receive<Types>...> selector{};
  return selector.Select(Receive{chans}...);
}

template <typename... Types>
auto TrySelect(Channel<Types>&... chans) {
  detail::Selector<true, Receive<Types>...> selector{};
  return selector.TrySelect(Receive{chans}...);
}

}  // namespace weave::fibers
