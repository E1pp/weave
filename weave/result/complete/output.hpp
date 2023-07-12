#pragma once

#include <weave/result/types/unit.hpp>

#include <type_traits>

namespace weave::result {

// (InvokeArgs -> non-void T) -> (InvokeArgs -> T)
// (InvokeArgs -> void) -> (InvokeArgs -> Unit)
template <typename Invokable>
struct CompleteOutput {
  explicit CompleteOutput(Invokable invokable)
      : invokable_(std::move(invokable)) {
  }

  template <typename... InvokeArgs>
  decltype(auto) operator()(InvokeArgs... args) {
    using InitialInvokeType = std::invoke_result_t<Invokable, InvokeArgs...>;

    if constexpr (std::is_same_v<InitialInvokeType, void>) {
      invokable_(std::move(args)...);
      return Unit{};

    } else {
      return invokable_(std::move(args)...);
    }
  }

 private:
  Invokable invokable_;
};

} // namespace weave::result