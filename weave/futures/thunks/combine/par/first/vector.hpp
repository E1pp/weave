#pragma once

#include <weave/futures/thunks/combine/par/first/decl.hpp>

#include <weave/futures/thunks/combine/par/detail/sync_strategies/join_all.hpp>
#include <weave/futures/thunks/combine/par/detail/storage_types/vector.hpp>
#include <weave/futures/thunks/combine/par/detail/join_block.hpp>

#include <weave/futures/traits/value_of.hpp>

#include <weave/result/make/err.hpp>
#include <weave/result/make/ok.hpp>

// #include <weave/futures/thunks/detail/cancel_base.hpp>

#include <optional>

namespace weave::futures::thunks {

// Default is OnHeap == true
template <bool OnHeap, typename Cons, SomeFuture Future>
class FirstControlBlock<OnHeap, Cons, detail::TaggedVector, Future> final: public detail::JoinBlock<true,  FirstControlBlock<true, Cons, detail::TaggedVector, Future>, detail::JoinAll<true>, traits::ValueOf<Future>, Cons, detail::TaggedVector, Future>{
 public:
  using ValueType = traits::ValueOf<Future>;
  using Base = detail::JoinBlock<true,  FirstControlBlock<true, Cons, detail::TaggedVector, Future>, detail::JoinAll<true>, ValueType, Cons, detail::TaggedVector, Future>;
 
  template<typename InterStorage>
  requires std::is_constructible_v<Base, Cons&, InterStorage>
  FirstControlBlock(size_t, Cons& cons, InterStorage storage) : Base(cons, std::move(storage)){
  }

  ~FirstControlBlock() override = default;

  void Consume(Output<ValueType> out, size_t) {
    auto result = std::move(out.result);

    // CompleteConsumer may throw
    wheels::Defer cleanup([&] {
      Base::ReleaseRef();
    });

    if (result && Base::MarkFulfilled()) {
      // we are the first result
      Base::CompleteConsumer(std::move(result));
    }

    if (bool should_complete = Base::ProducerDone()) {
      Base::CompleteConsumer(std::move(result));
    }
  }

  void Cancel() {
    // CancelConsumer() may throw
    wheels::Defer cleanup([&] {
      Base::ReleaseRef();
    });

    if (bool should_complete = Base::ProducerDone()) {
      Base::CancelConsumer();
    }
  }
};

///////////////////////////////////////////////////////////////////////

// OnStack spec which waits for the last future to cancel
template <typename Cons, SomeFuture Future>
class FirstControlBlock<false, Cons, detail::TaggedVector, Future> final: public detail::JoinBlock<false,  FirstControlBlock<false, Cons, detail::TaggedVector, Future>, detail::JoinAll<false>, traits::ValueOf<Future>, Cons, detail::TaggedVector, Future> {
 public:
  using ValueType = traits::ValueOf<Future>;
  using Base = detail::JoinBlock<false,  FirstControlBlock<false, Cons, detail::TaggedVector, Future>, detail::JoinAll<false>, ValueType, Cons, detail::TaggedVector, Future>;
 
  template<typename InterStorage>
  requires std::is_constructible_v<Base, Cons&, InterStorage>
  FirstControlBlock(size_t, Cons& cons, InterStorage storage) : Base(cons, std::move(storage)){
  }

  ~FirstControlBlock() override = default;

  void Consume(Output<ValueType> out, size_t) {
    auto result = std::move(out.result);

    if (result && Base::MarkFulfilled()) {
      // we are the first result
      EmplaceResult(std::move(result));
    }

    if (bool is_the_last = Base::ProducerDone()) {
      // we are the last one
      if (res_) {
        Base::CompleteConsumer(std::move(*res_));
      } else {
        Base::CompleteConsumer(std::move(result));
      }
    }
  }

  void Cancel() {
    if (bool is_the_last = Base::ProducerDone()) {
      if (res_) {
        Base::CompleteConsumer(std::move(*res_));
      } else {
        Base::CancelConsumer();
      }
    }
  }

 private:
  void EmplaceResult(Result<ValueType> res) {
    // Emplace result
    res_.emplace(std::move(res));

    // Cancel the rest
    Base::Forward(cancel::Signal::Cancel());
  }

 private:
  std::optional<Result<ValueType>> res_;
};


// // OnStack spec which waits for the last future to cancel
// template <SomeFuture Future>
// struct FirstControlBlock<false, detail::Vector, Future>
//     : public detail::JoinBlock<false, traits::ValueOf<Future>,
//                                FirstControlBlock<false, detail::Vector, Future>,
//                                detail::JoinAllOnHeap, detail::Vector, Future>,
//       public detail::VariadicCancellableBase<Future> {
//  public:
//   using ValueType = traits::ValueOf<Future>;
//   using Base =
//       detail::JoinBlock<false, ValueType,
//                         FirstControlBlock<false, detail::Vector, Future>,
//                         detail::JoinAllOnHeap, detail::Vector, Future>;

//   using Base::Base;

//   // Non-copyable
//   FirstControlBlock(const FirstControlBlock&) = delete;
//   FirstControlBlock& operator=(const FirstControlBlock&) = delete;

//   // Movable
//   FirstControlBlock(FirstControlBlock&&) = default;
//   FirstControlBlock& operator=(FirstControlBlock&&) = default;

//   ~FirstControlBlock() override = default;

//   void Create() {
//     // No-Op
//   }

//   void Consume(Output<ValueType> out, size_t) {
//     auto result = std::move(out.result);

//     if (result && Base::MarkFulfilled()) {
//       // we are the first result
//       EmplaceResult(std::move(result));
//     }

//     if (bool is_the_last = Base::ProducerDone()) {
//       // we are the last one
//       if (res_) {
//         Base::CompleteConsumer(std::move(*res_));
//       } else {
//         Base::CompleteConsumer(std::move(result));
//       }
//     }
//   }

//   void Cancel() {
//     if (bool is_the_last = Base::ProducerDone()) {
//       if (res_) {
//         Base::CompleteConsumer(std::move(*res_));
//       } else {
//         Base::CancelConsumer();
//       }
//     }
//   }

//  private:
//   void EmplaceResult(Result<ValueType> res) {
//     // Emplace result
//     res_.emplace(std::move(res));

//     // Cancel the rest
//     Base::Forward(cancel::Signal::Cancel());
//   }

//  private:
//   std::optional<Result<ValueType>> res_;
// };

}  // namespace weave::futures::thunks