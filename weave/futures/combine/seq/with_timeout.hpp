#pragma once

#include <weave/futures/combine/par/select.hpp>

#include <weave/futures/combine/seq/and_then.hpp>

#include <weave/futures/make/after.hpp>

#include <weave/satellite/satellite.hpp>

namespace weave::futures {

namespace pipe {

struct [[nodiscard]] WithTimeout {
  timers::Delay delay_;

  static Error TimeoutError() {
    return std::make_error_code(std::errc::timed_out);
  }

  explicit WithTimeout(timers::Delay delay)
      : delay_(delay) {
  }

  template <SomeFuture InputFuture>
  Future<traits::ValueOf<InputFuture>> auto Pipe(InputFuture f) {
    using V = traits::ValueOf<InputFuture>;

    return futures::Select(std::move(f), futures::After(delay_)) |
           futures::AndThen([](std::variant<V, Unit> variant) -> Result<V> {
             switch (variant.index()) {
               case 0: {
                 // f is done before the deadline
                 return result::Ok(std::move(std::get<0>(variant)));
               }
               case 1: {
                 // f timed out
                 return result::Err(TimeoutError());
               }
               default: {
                 WHEELS_PANIC("Unreachable!");
                 return result::Err(TimeoutError());
               }
             }
           });
  }

  template <traits::Cancellable InputFuture>
  Future<traits::ValueOf<InputFuture>> auto Pipe(InputFuture f) {
    using V = traits::ValueOf<InputFuture>;

    return futures::no_alloc::Select(std::move(f), futures::After(delay_)) |
           futures::AndThen([](std::variant<V, Unit> variant) -> Result<V> {
             switch (variant.index()) {
               case 0: {
                 // f is done before the deadline
                 return result::Ok(std::move(std::get<0>(variant)));
               }
               case 1: {
                 // f timed out
                 return result::Err(TimeoutError());
               }
               default: {
                 WHEELS_PANIC("Unreachable!");
                 return result::Err(TimeoutError());
               }
             }
           });
  }
};

}  // namespace pipe

// Future<T> -> Delay -> Future<T>

inline auto WithTimeout(timers::Delay delay) {
  return pipe::WithTimeout{delay};
}

inline auto WithTimeout(timers::Millis delay) {
  auto* global_proc = satellite::GetProcessor();

  WHEELS_VERIFY(global_proc != nullptr,
                "Use satellite::MakeVisible before calling this overload!");

  return pipe::WithTimeout{global_proc->DelayFromThis(delay)};
}

}  // namespace weave::futures