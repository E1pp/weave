#pragma once

#include <weave/result/types/result.hpp>

namespace weave::result {

/*
 *
 * Usage:
 *
 * futures::Just() | futures::AndThen([](Unit) -> Result<int> {
 *   return result::Err(Timeout());
 * });
 *
 */

inline auto Err(Error error) {
  return std::unexpected(std::move(error));
}

}  // namespace weave::result
