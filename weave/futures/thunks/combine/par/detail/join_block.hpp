#pragma once

#include <weave/cancel/sources/join.hpp>

#include <weave/futures/old_types/future.hpp>

#include <weave/threads/lockfree/count_and_flags.hpp>

#include <wheels/core/defer.hpp>

namespace weave::futures::thunks::detail {

template <typename S, typename T>
concept StorageFor = requires(S s, T* t) {
  { s.BootUpFutures(t) } -> std::same_as<void>;
  { s.Size() } -> std::same_as<size_t>;
};

template <typename T>
concept SyncStrategy = std::is_default_constructible_v<T> &&
    requires(T t, uint32_t val) {
  { t.TryFulfill() } -> std::same_as<bool>;
  { t.ProducerDone() } -> std::same_as<bool>;
  { t.SetProducerCount(val) } -> std::same_as<void>;
};

// Ref count >= Producer count
template <bool OnHeap, typename ValType, typename Combinator,
          SyncStrategy Strategy, template <typename...> typename StorageType,
          SomeFuture... Futures>
class JoinBlock : public cancel::sources::JoinSource<OnHeap> {
 public:
  using Storage = StorageType<Combinator, Futures...>;
  using CancelBase = cancel::sources::JoinSource<OnHeap>;

  using ValueType = ValType;

  template <typename... Args>
  requires std::is_constructible<Storage, Args...>::value explicit JoinBlock(
      size_t capacity, Args&&... args)
      : CancelBase(capacity),
        storage_(std::forward<Args>(args)...) {
  }

  // Non-copyable
  JoinBlock(const JoinBlock&) = delete;
  JoinBlock& operator=(const JoinBlock&) = delete;

  // Movable
  JoinBlock(JoinBlock&&) = default;
  JoinBlock& operator=(JoinBlock&&) = default;

  ~JoinBlock() override {
    delayed_state_.Destroy();
  }

  void Start(IConsumer<ValueType>* consumer) {
    static_assert(StorageFor<Storage, Combinator>);

    const size_t num_producers = storage_.Size();

    // Initialize self
    static_cast<Combinator*>(this)->Create();

    // Initialize CancelBase
    CancelBase::Create(num_producers);
    CancelBase::AddRef(num_producers);

    // Initialize sync algorithm
    delayed_state_.Create();
    StratRef().SetProducerCount(num_producers);

    consumer_ = consumer;

    // Add ref for cancel source
    CancelBase::AddRef(1);
    consumer->CancelToken().Attach(this);

    storage_.BootUpFutures(static_cast<Combinator*>(this));
  }

  cancel::Token CancelToken() {
    return cancel::Token::Fabricate(this);
  }

  template <typename T = int, std::enable_if_t<OnHeap, T> = 0>
  void CompleteConsumer(Result<ValueType> r) {
    if (CancelBase::CancelRequested()) {
      consumer_->Cancel(Context{});
    } else {
      consumer_->CancelToken().Detach(this);

      // Become the cancel source
      CancelBase::AddRef(1);
      CancelBase::Forward(cancel::Signal::Cancel());

      consumer_->Complete(std::move(r));
    }
  }

  template <typename T = int, std::enable_if_t<!OnHeap, T> = 0>
  void CompleteConsumer(Result<ValueType> r) {
    if (consumer_->CancelToken().CancelRequested()) {
      consumer_->Cancel(Context{});
    } else {
      consumer_->CancelToken().Detach(this);

      // Cancel was already propagated
      // before the result emplacement

      consumer_->Complete(std::move(r));
    }
  }

  void CancelConsumer() {
    consumer_->Cancel(Context{});
  }

  // true if first to ask
  bool MarkFulfilled() {
    return StratRef().TryFulfill();
  }

  // true if need to fulfill consumer
  bool ProducerDone() {
    return StratRef().ProducerDone();
  }

 private:
  Strategy& StratRef() {
    return delayed_state_.Refer();
  }

 private:
  IConsumer<ValueType>* consumer_;
  Storage storage_;

  // sync
  support::MaybeDelayed<!OnHeap, Strategy> delayed_state_;
};

}  // namespace weave::futures::thunks::detail