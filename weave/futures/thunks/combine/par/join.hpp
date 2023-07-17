#pragma once

#include <weave/futures/model/evaluation.hpp>

#include <weave/support/constructor_bases.hpp>

// #include <weave/futures/thunks/detail/cancel_base.hpp>

namespace weave::futures::thunks {

// OnHeap == true
template <
    bool OnHeap, typename ValType,
    template <bool, typename, template <typename...> typename, typename...>
    typename Block,
    typename Storage, template <typename...> typename EvalStorage,
    Thunk... Futures>
struct [[nodiscard]] Join final : public support::NonCopyableBase {
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
    using ControlBlock = Block<OnHeap, Cons, EvalStorage, Futures...>;

    explicit EvaluationFor(Join fut, Cons& cons)
        : block_(new ControlBlock(fut.maybe_treshold_, cons, std::move(fut.futures_))) {
    }

    void Start() {
      block_->Start();
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

// template <
//     typename ValType,
//     template <bool, typename, template <typename...> typename, typename...>
//     typename Block,
//     typename Storage, template <typename...> typename EvalStorage,
//     Thunk... Futures>
// struct [[nodiscard]] Join<false, ValType, Block, Storage, EvalStorage,
//                           Futures...>
//     final : public support::NonCopyableBase {
//  public:
//   using ValueType = ValType;

//   explicit Join(size_t size, Futures... fs)
//       : size_(size),
//         futures_(std::move(fs)...) {
//   }

//   // Movable
//   Join(Join&& that) noexcept
//       : size_(that.size_),
//         futures_(std::move(that.futures_)) {
//   }
//   Join& operator=(Join&&) = delete;

//  private:
//   template <Consumer<ValueType> Cons>
//   class EvaluationFor final : public support::PinnedBase {
//    public:
//     using ControlBlock = Block<false, Cons, EvalStorage, Futures...>;

//     explicit EvaluationFor(Join fut, Cons& cons)
//         : block_(fut.size_, cons, std::move(fut.futures_)) {
//     }

//     void Start() {
//       block_.Start();
//     }

//    private:
//     ControlBlock block_;
//   };

//  public:
//   template <Consumer<ValueType> Cons>
//   Evaluation<Join, Cons> auto Force(Cons& cons) {
//     return EvaluationFor<Cons>(std::move(*this), cons);
//   }

//  private:
//   size_t size_;
//   Storage futures_;
// };

}  // namespace weave::futures::thunks