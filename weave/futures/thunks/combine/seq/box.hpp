#pragma once

#include <weave/futures/model/evaluation.hpp>

#include <weave/support/constructor_bases.hpp>

namespace weave::futures::thunks {

template <typename T>
class Boxed;

namespace detail {

template <typename T>
struct IErasedFuture {
  virtual ~IErasedFuture() = default;

  virtual void RequestOutput(AbstractConsumer<T>*) = 0;
};

template <Thunk Future>
class TemplateSender final : public IErasedFuture<typename Future::ValueType>,
                             public support::PinnedBase {
 public:
  using T = typename Future::ValueType;

  explicit TemplateSender(Future fut)
      : eval_(std::move(fut).Force(*this)) {
  }

  ~TemplateSender() override final = default;

  // Completable<T>
  void Consume(Output<T> o) noexcept {
    receiver_->Consume(std::move(o));
  }

  // CancelSource
  void Cancel(Context ctx) noexcept {
    receiver_->Cancel(std::move(ctx));
  }

  cancel::Token CancelToken() {
    return receiver_->CancelToken();
  }

 private:
  void RequestOutput(AbstractConsumer<T>* rec) override final {
    receiver_ = rec;
    eval_.Start();
  }

 private:
  AbstractConsumer<T>* receiver_{nullptr};
  EvaluationType<TemplateSender, Future> eval_;
};

template <typename F, typename T>
concept NotBoxed = !std::is_same_v<F, Boxed<T>> && UnrestrictedThunk<F> &&
                   std::is_same_v<typename F::ValueType, T>;

}  // namespace detail

template <typename T>
class [[nodiscard]] Boxed final {
 public:
  using ValueType = T;

  // Non-copyable
  Boxed(const Boxed&) = delete;
  Boxed& operator=(const Boxed&) = delete;

  // Auto-boxing
  template <detail::NotBoxed<T> Future>
  Boxed(Future fut) {  // NOLINT
    erased_ = new detail::TemplateSender<Future>(std::move(fut));
  }

  // Movable
  Boxed(Boxed&& that) noexcept
      : erased_(std::exchange(that.erased_, nullptr)) {
  }
  Boxed& operator=(Boxed&&) = delete;

 private:
  template <Consumer<ValueType> Cons>
  class EvaluationFor final : public support::PinnedBase,
                              public AbstractConsumer<ValueType> {
    friend class Boxed;

    EvaluationFor(Boxed fut, Cons& cons)
        : sender_(std::exchange(fut.erased_, nullptr)),
          cons_(cons) {
    }

   public:
    void Start() {
      sender_->RequestOutput(this);
    }

    ~EvaluationFor() override final {
      if (sender_ == nullptr) {
        return;
      }

      delete sender_;
    }

   private:
    void Consume(Output<ValueType> o) noexcept override final {
      Complete(cons_, std::move(o));
    }

    void Cancel(Context ctx) noexcept override final {
      cons_.Cancel(std::move(ctx));
    }

    cancel::Token CancelToken() override final {
      return cons_.CancelToken();
    }

   private:
    detail::IErasedFuture<T>* sender_;
    Cons& cons_;
  };

 public:
  template <Consumer<ValueType> Cons>
  Evaluation<Boxed, Cons> auto Force(Cons& cons) {
    return EvaluationFor<Cons>(std::move(*this), cons);
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
