#pragma once

#include <weave/cancel/sources/join.hpp>

#include <weave/futures/types/future.hpp>

#include <weave/support/constructor_bases.hpp>

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

///////////////////////////////////////////////////////////////////////////////////

// Ref count >= Producer count
template <bool OnHeap, typename Combinator, SyncStrategy Strategy, typename V,
          typename Cons, template <typename...> typename StorageType,
          Thunk... Futures>
class JoinBlock : public cancel::sources::JoinSource<OnHeap> {
  using CancelBase = cancel::sources::JoinSource<OnHeap>;
  using Storage = StorageType<Combinator, Futures...>;
  using ValueType = V;

 public:
  template <typename InterStorage>
  requires std::is_constructible_v<Storage, InterStorage>
  explicit JoinBlock(Cons& cons, InterStorage storage)
      : CancelBase(storage.Size()),
        cons_(cons),
        storage_(std::move(storage)) {
  }

  ~JoinBlock() override = default;

  void Start() {
    const size_t num_producers = storage_.Size();

    // Prepare producers
    CancelBase::AddRef(num_producers);
    state_.SetProducerCount(num_producers);

    // Subscribe to cancellation
    CancelBase::AddRef(1);
    cons_.CancelToken().Attach(this);

    // Start futures
    storage_.BootUpFutures(static_cast<Combinator*>(this));
  }

  cancel::Token CancelToken() {
    return cancel::Token::Fabricate(this);
  }

  template <typename T = int, std::enable_if_t<OnHeap, T> = 0>
  void CompleteConsumer(Result<ValueType> r) {
    if (CancelBase::CancelRequested()) {
      cons_.Cancel(Context{});
    } else {
      cons_.CancelToken().Detach(this);

      // Become the cancel source
      CancelBase::AddRef(1);
      CancelBase::Forward(cancel::Signal::Cancel());

      Complete(cons_, std::move(r));
    }
  }

  template <typename T = int, std::enable_if_t<!OnHeap, T> = 0>
  void CompleteConsumer(Result<ValueType> r) {
    if (cons_.CancelToken().CancelRequested()) {
      cons_.Cancel(Context{});
    } else {
      cons_.CancelToken().Detach(this);

      // Cancel was already propagated
      // before the result emplacement

      Complete(cons_, std::move(r));
    }
  }

  void CancelConsumer() {
    cons_.Cancel(Context{});
  }

  // true if first to ask
  bool MarkFulfilled() {
    return state_.TryFulfill();
  }

  // true if need to fulfill consumer
  bool ProducerDone() {
    return state_.ProducerDone();
  }

 private:
  Cons& cons_;
  Storage storage_;

  Strategy state_{};
};

}  // namespace weave::futures::thunks::detail