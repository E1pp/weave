#pragma once

#include <weave/cancel/never.hpp>

#include <weave/futures/thunks/detail/shared_state.hpp>

#include <weave/futures/types/future.hpp>

namespace weave::futures::thunks::detail {

template <SomeFuture Future>
class StartState final : public SharedState<typename Future::ValueType>,
                         public IConsumer<typename Future::ValueType> {
 public:
  using T = typename Future::ValueType;
  using Base = SharedState<T>;

  explicit StartState(Future future)
      : future_(std::move(future)) {
    future_.Start(this);
  }

  virtual ~StartState() override final = default;

 private:
  void Consume(Output<T> out) noexcept override final {
    Base::Produce(std::move(out.result), out.context);
  }

  void Cancel(Context ctx) noexcept override final {
    Base::ProducerCancel(std::move(ctx));
  }

  cancel::Token CancelToken() override final {
    return cancel::Token::Fabricate(this);
  }

 private:
  Future future_;
};

}  // namespace weave::futures::thunks::detail