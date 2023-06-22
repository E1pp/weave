#pragma once

#include <tl/expected.hpp>

#include <weave/result/types/error.hpp>

namespace weave {

// https://github.com/TartanLlama/expected

template <typename T>
using Result = tl::expected<T, Error>;

}  // namespace weave
