#pragma once

#include <weave/cancel/sources/strand.hpp>

#include <weave/futures/model/thunk.hpp>

#include <weave/result/make/err.hpp>

#include <weave/threads/lockfree/rendezvous.hpp>

#include <wheels/core/defer.hpp>

#include <optional>

namespace weave::futures::thunks::detail {

template <typename T>
class SharedState : public cancel::sources::StrandSource {
  using Strand = cancel::sources::StrandSource;

  std::error_code PlaceholderError() {
    return std::make_error_code(static_cast<std::errc>(1337));
  }

 public:
  SharedState() {
    // Add ref for producer and consumer
    AddRef(2);
  }

  void Produce(Result<T> result, Context context = Context{}) {
    res_.emplace(Output<T>{std::move(result), context});

    wheels::Defer cleanup([&] {
      // Release producer ref
      ReleaseRef();
    });

    if (bool both = rendezvous_.Produce()) {
      Rendezvous();
    }
  }

  void ProducerCancel(Context ctx) {
    res_.emplace(Output<T>({result::Err(PlaceholderError()), std::move(ctx)}));

    wheels::Defer cleanup([&] {
      // Release producer ref
      ReleaseRef();
    });

    // Rendezvous with Consumer to call Cancel on them
    if (bool both = rendezvous_.Produce()) {
      Rendezvous();
    }
  }

  // When we add consumer, we Attach our token to theirs
  void Consume(AbstractConsumer<T>* consumer) {
    consumer_ = consumer;

    // add cancel source ref
    AddRef(1);
    consumer->CancelToken().Attach(this);

    // consumer->Cancel() may throw
    wheels::Defer cleanup([&] {
      // release consumer ref
      ReleaseRef();
    });

    if (bool both = rendezvous_.Consume()) {
      Rendezvous();
    }
  }

  ~SharedState() override = default;

 private:
  void Rendezvous() {
    if (consumer_->CancelToken().CancelRequested()) {
      consumer_->Cancel(std::move(res_->context));
    } else {
      consumer_->CancelToken().Detach(this);
      consumer_->Complete(std::move(*res_));
    }
  }

 private:
  // Producer-Consumer protocol
  std::optional<Output<T>> res_{};
  AbstractConsumer<T>* consumer_{};
  threads::lockfree::RendezvousStateMachine rendezvous_{};
};

}  // namespace weave::futures::thunks::detail