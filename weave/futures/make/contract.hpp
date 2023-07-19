#pragma once

#include <weave/futures/types/eager.hpp>

#include <weave/result/types/result.hpp>

#include <weave/result/make/ok.hpp>
#include <weave/result/make/err.hpp>

#include <tuple>

namespace weave::futures {

template <typename T>
class [[nodiscard]] Promise {
 public:
  using SharedState = thunks::detail::SharedState<T>;

  static inline Error PromiseDead(){
    return std::make_error_code(std::errc::operation_canceled);
  }

  explicit Promise(SharedState* state)
      : state_(state) {
  }

  // Non-copyable
  Promise(const Promise&) = delete;
  Promise& operator=(const Promise&) = delete;

  // Movable
  Promise(Promise&& that)
      : state_(that.Release()) {
  }

  void Set(Result<T> r) && {
    Release()->Produce(std::move(r));
  }

  void SetValue(T r) && {
    Release()->Produce(result::Ok(std::move(r)));
  }

  void SetError(Error e) && {
    Release()->Produce(result::Err(std::move(e)));
  }

  ~Promise(){
    if(state_ != nullptr){
      if(state_->CancelRequested()){
        // We were cancelled!
        Release()->ProducerCancel(Context{});
      } else {
        Release()->Produce(result::Err(PromiseDead()));
      }
    }
  }

 private:
  SharedState* Release() {
    return std::exchange(state_, nullptr);
  }

 private:
  SharedState* state_;
};

template <typename T>
std::tuple<ContractFuture<T>, Promise<T>> Contract() {
  auto state = new thunks::detail::SharedState<T>{};
  return {ContractFuture<T>(state), Promise<T>(state)};
}

}  // namespace weave::futures
