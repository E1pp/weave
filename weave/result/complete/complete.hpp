#pragma once

#include <weave/result/complete/input.hpp>
#include <weave/result/complete/output.hpp>
#include <weave/result/complete/wrap.hpp>

namespace weave::result {

template<typename InputType, typename Invokable>
class Complete {
 private:
  using ImplType = CompleteOutput<CompleteInput<InputType, Invokable>>;

 public:
  explicit Complete(Invokable f) : impl_(CompleteInput<InputType, Invokable>(std::move(f))){
  }

  decltype(auto) operator()(InputType input){
    return impl_(std::move(input));
  }

 private:
  ImplType impl_;
};

template<typename InputType, typename Invokable>
class Wrap {
 private:
  using ImplType = WrapOutput<CompleteInput<InputType, Invokable>>;

 public:
  explicit Wrap(Invokable f) : impl_(CompleteInput<InputType, Invokable>(std::move(f))){
  }

  decltype(auto) operator()(InputType input){
    return impl_(std::move(input));
  }

 private:
  ImplType impl_;
};

}  // namespace weave::result