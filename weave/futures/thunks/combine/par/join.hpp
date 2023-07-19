#pragma once

#include <weave/futures/model/evaluation.hpp>

#include <weave/futures/thunks/detail/cancel_base.hpp>

#include <weave/support/constructor_bases.hpp>

namespace weave::futures::thunks {

// OnHeap == true
template <
    bool Cancellable, bool OnHeap, typename ValType,
    template <bool, typename, template <typename...> typename, typename...>
    typename Block,
    typename Storage, template <typename...> typename EvalStorage,
    Thunk... Futures>
struct [[nodiscard]] Join final
    : public support::NonCopyableBase,
      public std::conditional_t<
          Cancellable, detail::JustCancellableBase<Storage>, detail::Empty> {
 public:
  using ValueType = ValType;

  template <typename... Args>
  requires std::is_constructible_v<Storage, Args...>
  explicit Join(size_t maybe_treshold, Args&&... args)
      : maybe_treshold_(maybe_treshold),
        futures_(std::forward<Args>(args)...) {
  }

  // Movable
  Join(Join&& that) noexcept
      : maybe_treshold_(that.maybe_treshold_),
        futures_(std::move(that.futures_)) {
  }
  Join& operator=(Join&&) = delete;

 private:
  template <Consumer<ValueType> Cons>
  class EvaluationFor final : public support::PinnedBase {
    using ControlBlock = Block<OnHeap, Cons, EvalStorage, Futures...>;

    friend struct Join;

    explicit EvaluationFor(Join fut, Cons& cons)
        : block_(new ControlBlock(fut.maybe_treshold_, cons,
                                  std::move(fut.futures_))) {
    }

   public:
    void Start() {
      std::exchange(block_, nullptr)->Start();
    }

    ~EvaluationFor(){
      if(block_ != nullptr){
        delete block_;
      }
    }

   private:
    ControlBlock* block_;
  };

 public:
  template <Consumer<ValueType> Cons>
  Evaluation<Join, Cons> auto Force(Cons& cons) {
    return EvaluationFor<Cons>(std::move(*this), cons);
  }

 private:
  size_t maybe_treshold_;
  Storage futures_;
};

////////////////////////////////////////////////////////////////////////

// OnHeap == false
template <
    bool Cancellable, typename ValType,
    template <bool, typename, template <typename...> typename, typename...>
    typename Block,
    typename Storage, template <typename...> typename EvalStorage,
    Thunk... Futures>
struct [[nodiscard]] Join<Cancellable, false, ValType, Block, Storage,
                          EvalStorage, Futures...>
    final
    : public support::NonCopyableBase,
      public std::conditional_t<Cancellable, detail::Full, detail::Empty> {
 public:
  using ValueType = ValType;

  template <typename... Args>
  requires std::is_constructible_v<Storage, Args...>
  explicit Join(size_t maybe_treshold, Args&&... args)
      : maybe_treshold_(maybe_treshold),
        futures_(std::forward<Args>(args)...) {
  }

  // Movable
  Join(Join&& that) noexcept
      : maybe_treshold_(that.maybe_treshold_),
        futures_(std::move(that.futures_)) {
  }
  Join& operator=(Join&&) = delete;

 private:
  template <Consumer<ValueType> Cons>
  class EvaluationFor final : public support::PinnedBase {
   public:
    using ControlBlock = Block<false, Cons, EvalStorage, Futures...>;

    explicit EvaluationFor(Join fut, Cons& cons)
        : block_(fut.maybe_treshold_, cons, std::move(fut.futures_)) {
    }

    void Start() {
      block_.Start();
    }

   private:
    ControlBlock block_;
  };

 public:
  template <Consumer<ValueType> Cons>
  Evaluation<Join, Cons> auto Force(Cons& cons) {
    return EvaluationFor<Cons>(std::move(*this), cons);
  }

 private:
  size_t maybe_treshold_;
  Storage futures_;
};

}  // namespace weave::futures::thunks