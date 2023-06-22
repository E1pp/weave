#pragma once

#include <weave/result/types/result.hpp>
#include <weave/result/types/status.hpp>

namespace weave::result {

/*
 * Usage:
 *
 * futures::Value(1) | futures::AndThen([](int v) {
 *   return result::Ok(v + 1);
 * })
 *
 */

template <typename T>
Result<T> Ok(T value) {
  return {std::move(value)};
}

/*
 * Usage:
 *
 * futures::Just() | futures::AndThen([](Unit) {
 *   return result::Ok();
 * });
 *
 */

inline Status Ok() {
  return {Unit{}};
}

}  // namespace weave::result
