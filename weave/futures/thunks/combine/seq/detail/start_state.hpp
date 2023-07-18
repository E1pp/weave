#pragma once

#include <weave/futures/model/evaluation.hpp>

#include <weave/futures/thunks/detail/shared_state.hpp>

namespace weave::futures::thunks::detail {

template <Thunk Future>
class StartState final : public SharedState<typename Future::ValueType> {
 public:
  using T = typename Future::ValueType;
  using Base = SharedState<T>;

  explicit StartState(Future future)
      : eval_(std::move(future).Force(*this)) {
    eval_.Start();
  }

  virtual ~StartState() override final = default;

  void Start(){
    eval_.Start();
  }

  // Completable
  void Consume(Output<T> out) noexcept {
    Base::Produce(std::move(out.result), out.context);
  }

  // CancelSource
  void Cancel(Context ctx) noexcept {
    Base::ProducerCancel(std::move(ctx));
  }

  cancel::Token CancelToken() {
    return cancel::Token::Fabricate(this);
  }

 private:
  EvaluationType<StartState, Future> eval_;
};

}  // namespace weave::futures::thunks::detail