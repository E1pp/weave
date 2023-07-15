#pragma once

#include <weave/futures/old_model/thunk.hpp>

#include <cstdlib>

namespace weave::futures::thunks {

namespace detail {

template <typename T>
class IErasedFuture {
 public:
  virtual void Start(IConsumer<T>*) = 0;

  virtual ~IErasedFuture() = default;
};

template <Thunk Future>
class TemplateFuture final : public IErasedFuture<typename Future::ValueType> {
 public:
  using ValueType = typename Future::ValueType;

  explicit TemplateFuture(Future future)
      : future_(std::move(future)) {
  }

  // Non-copyable
  TemplateFuture(const TemplateFuture&) = delete;
  TemplateFuture& operator=(const TemplateFuture&) = delete;

  // Movable
  TemplateFuture(TemplateFuture&& that)
      : future_(std::move(that.future)) {
  }
  TemplateFuture& operator=(TemplateFuture&&) = default;

  void Start(IConsumer<ValueType>* consumer) override final {
    future_.Start(consumer);
  }

  ~TemplateFuture() override = default;

 private:
  Future future_;
};

}  // namespace detail

template <typename T>
class [[nodiscard]] Boxed {
 public:
  using ValueType = T;

  // Auto-boxing
  template <Thunk Thunk>
  Boxed(Thunk future) {  // NOLINT
    erased_ = new detail::TemplateFuture<Thunk>(std::move(future));
  }

  // Non-copyable
  Boxed(const Boxed&) = delete;
  Boxed& operator=(const Boxed&) = delete;

  // Movable
  Boxed(Boxed&& that)
      : erased_(std::exchange(that.erased_, nullptr)) {
  }
  Boxed& operator=(Boxed&&) = default;

  void Start(IConsumer<T>* consumer) {
    erased_->Start(consumer);
  }

  ~Boxed() {
    if (erased_ == nullptr) {
      return;
    }

    delete erased_;
  }

 private:
  detail::IErasedFuture<T>* erased_{nullptr};
};

}  // namespace weave::futures::thunks
