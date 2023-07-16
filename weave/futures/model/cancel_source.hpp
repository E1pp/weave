#pragma once

#include <weave/cancel/token.hpp>

#include <weave/futures/model/context.hpp>

namespace weave::futures {

template <typename S>
concept CancelSource = requires(S& source, Context ctx){
  source.Cancel(std::move(ctx));
  requires noexcept(source.Cancel(std::move(ctx)));

  {source.CancelToken()} -> std::same_as<cancel::Token>;
};

} // namespace weave::futures