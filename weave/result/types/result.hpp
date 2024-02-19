#pragma once

#include <expected>

#include <weave/result/types/error.hpp>

namespace weave {

template <typename T>
using Result = std::expected<T, Error>;

}  // namespace weave
