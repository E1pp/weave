#pragma once

#include <weave/futures/model/cancel_source.hpp>
#include <weave/futures/model/output.hpp>

namespace weave::futures {

template <typename C, typename T>
concept Completable = requires(C& consumer, Output<T> o) {
  consumer.Consume(std::move(o));
};

template <typename T, Completable<T> C>
void Complete(C& consumer, Result<T> r) noexcept {
  consumer.Consume({std::move(r), Context{}});
}

template <typename T, Completable<T> C>
void Complete(C& consumer, Output<T> o) noexcept {
  consumer.Consume(std::move(o));
}

template <typename C, typename T>
concept Consumer = Completable<C, T> && CancelSource<C>;

//////////////////////////////////////////////////////////////////////////////

template <typename T>
struct AbstractConsumer {
  virtual ~AbstractConsumer() = default;

  virtual void Consume(Output<T>) noexcept = 0;

  void Complete(Output<T> o) {
    Consume(std::move(o));
  }

  void Complete(Result<T> r) {
    Consume({std::move(r), Context{}});
  }

  virtual void Cancel(Context) noexcept = 0;

  virtual cancel::Token CancelToken() = 0;
};

}  // namespace weave::futures
