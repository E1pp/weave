#pragma once

#include <weave/futures/combine/seq/via.hpp>

#include <weave/futures/thunks/detail/cancel_base.hpp>

#include <weave/result/make/err.hpp>

#include <optional>

namespace weave::futures::thunks {

// Cancellable if both are Cancellable
// is seemless thus no need for CancelRequested lookups
template <SomeFuture Future>
class Flattenned final
    : public IConsumer<typename Future::ValueType>,
      public detail::VariadicCancellableBase<Future,
                                             typename Future::ValueType> {
 public:
  using InnerType = typename Future::ValueType;
  using ValueType = typename InnerType::ValueType;

  explicit Flattenned(Future f)
      : future_(std::move(f)) {
  }

  // Non-copyable
  Flattenned(const Flattenned&) = delete;
  Flattenned& operator=(const Flattenned&) = delete;

  // Movable
  Flattenned(Flattenned&&) = default;

  void Start(IConsumer<ValueType>* consumer) {
    consumer_ = consumer;
    future_.Start(this);
  }

 private:
  // Consume recieves Future it needs to wake up and
  // redirect our actual consumer to the awoken future
  void Consume(Output<InnerType> out) noexcept override final {
    auto future = std::move(out.result);
    if (future) {
      // future's lifetime is scope of this Consume
      // however, Start may require future to stay alive for as long
      // as flattened exists
      // we also have to enforce that newly created future has the correct
      // executor
      inner_future_.emplace(
          std::move(*future) |
          futures::Via(*out.context.executor_, out.context.hint_));
      inner_future_->Start(consumer_);
    } else {
      consumer_->Complete(Output<ValueType>{
          result::Err(std::move(future.error())), out.context});
    }
  }

  void Cancel(Context ctx) noexcept override final {
    consumer_->Cancel(std::move(ctx));
  }

  cancel::Token CancelToken() override final {
    return consumer_->CancelToken();
  }

 private:
  Future future_;
  std::optional<thunks::Via<InnerType>> inner_future_;
  IConsumer<ValueType>* consumer_;
};

}  // namespace weave::futures::thunks