#pragma once

#include <weave/fibers/sched/suspend.hpp>

#include <optional>

namespace weave::fibers::experimental::detail {

enum class State : uint64_t { Receiver = 0, Sender = 1, Broken = 2, Null = 3 };

///////////////////////////////////////////////////////////////

template <typename T>
struct IWaiter {
  virtual void SetFiber(FiberHandle) = 0;
  virtual void Schedule() = 0;
  virtual void Switch() = 0;

  virtual State GetStatus() = 0;

  virtual T ReadValue() = 0;
  virtual bool WriteValue(T&) = 0;

  virtual ~IWaiter() = default;
};

///////////////////////////////////////////////////////////////

template <typename T>
class Waiter final : public IWaiter<T> {
 public:
  explicit Waiter(State st)
      : status_(st) {
  }

  void SetFiber(FiberHandle handle) override final {
    handle_ = handle;
  }

  void Schedule() override final {
    handle_.Schedule(executors::SchedulerHint::Next);
  }

  void Switch() override final {
    handle_.Switch();
  }

  State GetStatus() override final {
    return status_;
  }

  T ReadValue() override final {
    return std::move(*storage_);
  }

  bool WriteValue(T& val) override final {
    storage_.emplace(std::move(val));
    return true;
  }

  virtual ~Waiter() = default;

 private:
  State status_;
  FiberHandle handle_{FiberHandle::Invalid()};
  std::optional<T> storage_{};
};

///////////////////////////////////////////////////////////////

}  // namespace weave::fibers::experimental::detail