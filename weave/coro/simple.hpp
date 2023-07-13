#pragma once

#include <weave/coro/core.hpp>

#include <twist/ed/local/ptr.hpp>

#include <exception>
#include <optional>

namespace weave::coro {

class SimpleCoroutine {
 public:
  template <typename Function>
  explicit SimpleCoroutine(Function function)
      : stack_(sure::Stack::AllocateBytes(kDefaultStackSize)),
        impl_(
            [owner = this, function = std::move(function)]() mutable noexcept {
              try {
                function();
              } catch (...) {
                owner->exception_container_ = std::current_exception();
              }

              owner->is_completed_ = true;
            },
            &stack_) {
  }

  void Resume();

  static SimpleCoroutine* Self();

  // Suspend running coroutine
  static void Suspend();

  bool IsCompleted() const;

 private:
  std::optional<std::exception_ptr> exception_container_{std::nullopt};
  sure::Stack stack_;
  Coroutine impl_;
  bool is_completed_{false};

  static inline twist::ed::ThreadLocalPtr<SimpleCoroutine> currently_active{nullptr};
};

}  // namespace weave::coro
