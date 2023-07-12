#pragma once

#include <weave/result/make/ok.hpp>

#include <weave/result/traits/value_of.hpp>

#include <type_traits>

namespace weave::result {

template <typename T>
concept SomeResult =
    std::is_same_v<Result<typename result::traits::ValueOf<T>>, T>;

// (InvokeArgs -> Result<T>) -> (InvokeArgs -> Result<T>)
// (InvokeArgs -> non-void T) -> (InvokeArgs -> Result<T>)
// (InvokeArgs -> void) -> (InvokeArgs -> Result<Unit>)
template <typename Invokable>
struct WrapOutput {
  explicit WrapOutput(Invokable invokable)
      : invokable_(std::move(invokable)) {
  }

  template <typename... InvokeArgs>
  decltype(auto) operator()(InvokeArgs... args) {
    using InitialInvokeType = std::invoke_result_t<Invokable, InvokeArgs...>;

    if constexpr (SomeResult<InitialInvokeType>) {
      // invokable_ returns Result<T>
      return invokable_(std::move(args)...);
    } else {
      // invokable_ returns unwrapped value
      if constexpr (std::is_same_v<InitialInvokeType, void>) {
        invokable_(std::move(args)...);
        return result::Ok();

      } else {
        return result::Ok(std::move(invokable_(std::move(args)...)));
      }
    }
  }

 private:
  Invokable invokable_;
};

}  // namespace weave::result