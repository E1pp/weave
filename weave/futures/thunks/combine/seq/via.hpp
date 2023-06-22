#pragma once

#include <weave/executors/executor.hpp>

#include <weave/futures/thunks/detail/cancel_base.hpp>

namespace weave::futures::thunks {

// Via is seemless thus no need for lookup of CancelRequested inside to be
// Cancellable
template <SomeFuture Future>
class [[nodiscard]] Via final : public IConsumer<typename Future::ValueType>,
                                public detail::CancellableBase<Future> {
 public:
  using ValueType = typename Future::ValueType;

  Via(Future f, Context context)
      : future_(std::move(f)),
        next_context_(context) {
  }

  // Non-copyable
  Via(const Via&) = delete;
  Via& operator=(const Via&) = delete;

  // Movable
  Via(Via&&) = default;

  void Start(IConsumer<ValueType>* consumer) {
    consumer_ = consumer;
    future_.Start(this);
  }

 private:
  void Consume(Output<ValueType> out) noexcept override final {
    out.context = next_context_;
    consumer_->Complete(std::move(out));
  }

  // insert your own context
  void Cancel(Context) noexcept override final {
    consumer_->Cancel(std::move(next_context_));
  }

  cancel::Token CancelToken() override final {
    // forward down the chain
    return consumer_->CancelToken();
  }

 private:
  IConsumer<ValueType>* consumer_;
  Future future_;
  Context next_context_;
};

}  // namespace weave::futures::thunks