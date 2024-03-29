#pragma once

#include <weave/cancel/sources/fork.hpp>

#include <weave/futures/model/evaluation.hpp>

#include <weave/threads/lockfree/rendezvous.hpp>

#include <weave/futures/thunks/detail/cancel_base.hpp>

#include <weave/support/constructor_bases.hpp>

#include <wheels/core/defer.hpp>

#include <array>
#include <optional>

namespace weave::futures::thunks {

template <size_t NumTines, class Future>
class Forker;

///////////////////////////////////////////////////////////////////////////////////

template <size_t Index, size_t NumTines, class Future>
class [[nodiscard]] Tine final : public support::NonCopyableBase,
                                 public detail::CancellableBase<Future> {
 private:
  using ForkerType = Forker<NumTines, Future>;

 public:
  using ValueType = typename ForkerType::V;

  explicit Tine(ForkerType* forker)
      : forker_(forker) {
  }

  // Movable
  Tine(Tine&& that) noexcept
      : forker_(that.Release()) {
  }
  Tine& operator=(Tine&&) = delete;

 private:
  template <Consumer<ValueType> Cons>
  class EvaluationFor final : public support::PinnedBase,
                              public AbstractConsumer<ValueType> {
    template <size_t Count, class F>
    friend class Forker;

    friend class Tine;

    EvaluationFor(Tine fut, Cons& cons)
        : cons_(cons),
          forker_(fut.Release()) {
    }

   public:
    void Start() {
      Release()->template Start<Index>(this);
    }

    ~EvaluationFor(){
      if(forker_ != nullptr){
        Release()->template DropTine<Index>();
      }
    }

   private:
    // AbstractConsumer
    void Consume(Output<ValueType> o) noexcept override final {
      cons_.Consume(std::move(o));
    }

    void Cancel(Context ctx) noexcept override final {
      cons_.Cancel(std::move(ctx));
    }

    cancel::Token CancelToken() override final {
      return cons_.CancelToken();
    }

   private:
    ForkerType* Release() {
      return std::exchange(forker_, nullptr);
    }

   private:
    Cons& cons_;
    ForkerType* forker_;
  };

 public:
  template <Consumer<ValueType> Cons>
  Evaluation<Tine, Cons> auto Force(Cons& cons) {
    return EvaluationFor<Cons>(std::move(*this), cons);
  }

  ~Tine(){
    if(forker_ != nullptr){
      Release()->template DropTine<Index>();
    }
  }

 private:
  ForkerType* Release() {
    return std::exchange(forker_, nullptr);
  }

 private:
  ForkerType* forker_;
};

///////////////////////////////////////////////////////////////////////////////////

template <size_t NumTines, class Future>
class Forker final
    : public support::PinnedBase,
      public cancel::sources::ForkSource<NumTines> {
 private:
  template <size_t Index, size_t N, class F>
  friend class Tine;

  using V = typename Future::ValueType;
  using Base = cancel::sources::ForkSource<NumTines>;

  struct Rendezvous {
    AbstractConsumer<V>* consumer_{nullptr};
    threads::lockfree::RendezvousStateMachine rendezvous_;
  };

 public:
  // Add Refs for every consumer
  explicit Forker(Future f)
      : eval_(std::move(f).Force(*this)) {
    Base::AddRef(NumTines);
  }

  auto MakeTines() {
    return [forker = this]<size_t... Inds>(std::index_sequence<Inds...>) {
      return std::make_tuple(Tine<Inds, NumTines, Future>(forker)...);
    }
    (std::make_index_sequence<NumTines>());
  }

  template <size_t Index>
  void Start(AbstractConsumer<V>* consumer) {
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

  // Completable
  void Consume(Output<V> out) noexcept {
    wheels::Defer cleanup([&] {
      Base::ReleaseRef();
    });

    output_.emplace(std::move(out));

    // Iterate over array in compile time
    [&]<size_t... IthConsumer>(std::index_sequence<IthConsumer...>) {
      (TryRendezvousWith<IthConsumer>(), ...);
    }
    (std::make_index_sequence<NumTines>());
  }

  // CancelSource
  void Cancel(Context ctx) noexcept {
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
      if(auto* consumer = states_[i].consumer_){
        consumer->Cancel(ctx);
      }
    }
  }

  cancel::Token CancelToken() {
    return cancel::Token::Fabricate(this);
  }

  template <size_t Index>
  void DropTine(){
    states_[Index].consumer_ = nullptr;
    Base::template AsLeaf<Index>()->Forward(cancel::Signal::Cancel());
  }

 private:
  template <size_t Index>
  void TryRendezvousWith() {
    if (bool both = states_[Index].rendezvous_.Produce()) {
      DoRendezvous<Index>();
    }
  }

  template <size_t Index>
  void DoRendezvous() {
    // At this point consumer in index'th place is set
    AbstractConsumer<V>* consumer = states_[Index].consumer_;
    Output<V> out = *output_;

    if (consumer->CancelToken().CancelRequested()) {
      consumer->Cancel(std::move(out.context));
    } else {
      consumer->CancelToken().Detach(Base::template AsLeaf<Index>());
      consumer->Complete(std::move(out));
    }
  }

  void TryStart() {
    // We can only add ref for consumer here since TryStart
    //  might never be called if all forker tines are dropped
    // adding consumer ref any earlier would cause a memory leak!
    if (!started_.exchange(true, std::memory_order::relaxed)) {
      Base::AddRef(1);
      eval_.Start();
    }
  }

 private:
  EvaluationType<Forker, Future> eval_;
  twist::ed::stdlike::atomic<bool> started_{false};

  std::array<Rendezvous, NumTines> states_;
  std::optional<Output<V>> output_;
};

}  // namespace weave::futures::thunks