#pragma once

#include <weave/futures/combine/seq/via.hpp>

#include <weave/futures/thunks/detail/cancel_base.hpp>

#include <weave/result/make/err.hpp>

#include <weave/support/constructor_bases.hpp>

#include <optional>

namespace weave::futures::thunks {

// Cancellable if both are Cancellable
// is seemless thus no need for CancelRequested lookups
template <Thunk Future>
class Flattenned final : public support::NonCopyableBase, public detail::VariadicCancellableBase<Future, typename Future::ValueType> {
 public:
  using InnerType = typename Future::ValueType;
  using ValueType = typename InnerType::ValueType;

  explicit Flattenned(Future f)
      : future_(std::move(f)) {
  }

  // Movable
  Flattenned(Flattenned&& that) noexcept
      : future_(std::move(that.future_)) {
  }

 private:
  template <Consumer<ValueType> Cons>
  class EvaluationFor final : public support::PinnedBase {
   public:
    EvaluationFor(Flattenned fut, Cons& cons)
        : consumer_(cons),
          outer_eval_(std::move(fut.future_).Force(*this)) {
    }

    void Start() {
      outer_eval_.Start();
    }

    // Completable
    // Consume recieves Future it needs to wake up and
    // redirect our actual consumer to the awoken future
    void Consume(Output<InnerType> out) noexcept {
      auto inner = std::move(out.result);

      if (inner) {
        need_delete_ = true;
        auto future = std::move(*inner) |
                      futures::Via(*out.context.executor_, out.context.hint_);

        new (&inner_eval_) auto(std::move(future).Force(consumer_));
        inner_eval_.Start();
      } else {
        Complete<ValueType>(
            consumer_, {result::Err(std::move(inner.error())), out.context});
      }
    }

    // CancelSource
    void Cancel(Context ctx) noexcept {
      consumer_.Cancel(std::move(ctx));
    }

    cancel::Token CancelToken() {
      return consumer_.CancelToken();
    }

    ~EvaluationFor() {
      if (need_delete_) {
        std::destroy_at(&inner_eval_);
      }
    }

   private:
    Cons& consumer_;
    union {
      EvaluationType<Cons, thunks::Via<InnerType>> inner_eval_;
    };
    bool need_delete_{false};

    EvaluationType<EvaluationFor, Future> outer_eval_;
  };

 public:
  template <Consumer<ValueType> Cons>
  Evaluation<Flattenned, Cons> auto Force(Cons& cons) {
    return EvaluationFor<Cons>(std::move(*this), cons);
  }

 private:
  Future future_;
};

}  // namespace weave::futures::thunks