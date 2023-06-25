#pragma once

#include <weave/cancel/sources/fork.hpp>

#include <weave/threads/lockfree/rendezvous.hpp>

#include <weave/futures/thunks/detail/cancel_base.hpp>

#include <wheels/core/defer.hpp>

#include <array>
#include <optional>

namespace weave::futures::thunks {

template <size_t NumTines, SomeFuture Future>
class [[nodiscard]] Forker;

///////////////////////////////////////////////////////////////////////////////////

template <size_t Index, size_t NumTines, SomeFuture Future>
class [[nodiscard]] Tine : public detail::CancellableBase<Future> {
 private:
  using ForkerType = Forker<NumTines, Future>;

 public:
  using ValueType = typename ForkerType::V;

  explicit Tine(ForkerType* forker)
      : forker_(forker) {
  }

  ~Tine() {
    assert(forker_ == nullptr);
  }

  // Non-copyable
  Tine(const Tine&) = delete;
  Tine& operator=(const Tine&) = delete;

  // Movable
  Tine(Tine&& that)
      : forker_(that.Release()) {
  }
  Tine& operator=(Tine&&) = default;

  void Start(IConsumer<ValueType>* consumer) {
    Release()->template Start<Index>(consumer);
  }

 private:
  ForkerType* Release() {
    return std::exchange(forker_, nullptr);
  }

 private:
  ForkerType* forker_;
};

///////////////////////////////////////////////////////////////////////////////////

template <size_t NumTines, SomeFuture Future>
class [[nodiscard]] Forker final
    : public IConsumer<typename Future::ValueType>,
      public cancel::sources::ForkSource<NumTines> {
 private:
  template <size_t Index, size_t N, SomeFuture F>
  friend class Tine;

  using V = typename Future::ValueType;
  using Base = cancel::sources::ForkSource<NumTines>;

  struct Rendezvous {
    IConsumer<V>* consumer_{nullptr};
    threads::lockfree::RendezvousStateMachine rendezvous_;
    std::optional<Output<V>> output_;
  };

 public:
  // Add Refs for every consumer and producer
  explicit Forker(Future f)
      : future_(std::move(f)) {
    Base::AddRef(NumTines + 1);
  }

  // Non-copyable
  Forker(const Forker&) = delete;
  Forker& operator=(const Forker&) = delete;

  // Not-movable
  Forker(Forker&&) = delete;
  Forker& operator=(Forker&&) = delete;

  auto MakeTines() {
    return [forker = this]<size_t... Inds>(std::index_sequence<Inds...>) {
      return std::make_tuple(Tine<Inds, NumTines, Future>(forker)...);
    }
    (std::make_index_sequence<NumTines>());
  }

  template <size_t Index>
  void Start(IConsumer<V>* consumer) {
    states_[Index].consumer_ = consumer;

    Base::AddRef(1);
    consumer->CancelToken().Attach(Base::template AsLeaf<Index>());

    wheels::Defer cleanup([&] {
      Base::ReleaseRef();
    });

    if (bool both = states_[Index].rendezvous_.Consume()) {
      // Value is ready and we are free to consume it
      DoRendezvous<Index>();
    } else {
      // Value is not ready and we should try starting
      // input future in case we are the first one
      TryStart();
    }
  }

 private:
  void Consume(Output<V> out) noexcept override final {
    wheels::Defer cleanup([&] {
      Base::ReleaseRef();
    });

    // Iterate over array in compile time
    [&]<size_t... Inds>(std::index_sequence<Inds...>) {
      (ForkOutput<Inds>(out), ...);
    }
    (std::make_index_sequence<NumTines>());
  }

  void Cancel(Context ctx) noexcept override final {
    // We cancel underlying only if every consumer
    // decides to cancel us. Then we report
    // cancellation to everyone

    // Precondition implies that every consumer is already set
    // although they might be stuck at rendezvous_.Consume
    // in order to ensure that the only thing Start callers now do
    // is ReleaseRef, we do not rendezvous but
    // simply cancel every consumer then release our ref

    wheels::Defer cleanup([&] {
      Base::ReleaseRef();
    });

    for (size_t i = 0; i < NumTines; i++) {
      states_[i].consumer_->Cancel(ctx);
    }
  }

  cancel::Token CancelToken() override final {
    return cancel::Token::Fabricate(this);
  }

 private:
  template <size_t Index>
  void ForkOutput(Output<V> out) {
    // Emplace the result
    states_[Index].output_.emplace(out);

    // try rendezvous
    if (bool both = states_[Index].rendezvous_.Produce()) {
      DoRendezvous<Index>();
    }
  }

  template <size_t Index>
  void DoRendezvous() {
    // At this point consumer in index'th place is set
    IConsumer<V>* consumer = states_[Index].consumer_;
    std::optional<Output<V>>& storage = states_[Index].output_;

    if (consumer->CancelToken().CancelRequested()) {
      consumer->Cancel(std::move(storage->context));
    } else {
      consumer->CancelToken().Detach(Base::template AsLeaf<Index>());
      consumer->Complete(std::move(*storage));
    }
  }

  void TryStart() {
    if (!started_.exchange(true, std::memory_order::relaxed)) {
      future_.Start(this);
    }
  }

 private:
  Future future_;
  twist::ed::stdlike::atomic<bool> started_{false};

  std::array<Rendezvous, NumTines> states_;
};

}  // namespace weave::futures::thunks