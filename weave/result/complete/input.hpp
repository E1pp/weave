#pragma once

#include <weave/result/types/unit.hpp>

#include <type_traits>

namespace weave::result {

// (InputType -> InvokeResult) => (InputType -> InvokeResult)
// (void -> InvokeResult) => (InputType -> InvokeResult)
// Latter is done by discarding the argument
template <typename InputType, typename Invokable>
class InputAnyPromotion {
 public:
  explicit InputAnyPromotion(Invokable&& f)
      : f_(std::move(f)) {
  }

  decltype(auto) operator()(InputType input) {
    if constexpr (std::is_invocable_v<Invokable>) {
      return f_();
    } else {
      return f_(std::move(input));
    }
  }

 private:
  Invokable f_;
};

template <typename InputType, typename Invokable>
class InputVoidPromotion {
 public:
  explicit InputVoidPromotion(Invokable&& f)
      : f_(std::move(f)) {
  }

  decltype(auto) operator()(InputType input) {
    return f_(std::move(input));
  }

 private:
  Invokable f_;
};

template <typename Invokable>
class InputVoidPromotion<Unit, Invokable> {
 private:
  using ImplType = InputAnyPromotion<Unit, Invokable>;

 public:
  explicit InputVoidPromotion(Invokable&& f)
      : impl_(std::move(f)) {
  }

  decltype(auto) operator()(Unit) {
    return impl_({});
  }

 private:
  ImplType impl_;
};

#if defined(__WEAVE_AUTOCOMPLETE__)
template<typename InputType, typename Invokable>
using CompleteInput = InputAnyPromotion<InputType, Invokable>;
#else
template<typename InputType, typename Invokable>
using CompleteInput = InputVoidPromotion<InputType, Invokable>;
#endif

}  // namespace weave::result