#pragma once

#include <weave/result/types/error.hpp>

#include <weave/threads/lockfree/tagged_buffer.hpp>

namespace weave::cancel {

class Token;

//////////////////////////////////////////////////////////////

// (Not so) proudly taken from the await lib

class Signal {
  enum class Either {
    Cancel,
    Release,
  };

 public:
  static Signal Cancel() {
    return Signal{Either::Cancel};
  }

  static Signal Release() {
    return Signal{Either::Release};
  }

  bool CancelRequested() const {
    return either_ == Either::Cancel;
  }

  bool Released() const {
    return either_ == Either::Release;
  }

 private:
  explicit Signal(Either either)
      : either_(either) {
  }

 private:
  Either either_;
};

//////////////////////////////////////////////////////////////

/*
Token::Attach is always in hb with Forward by design

Since Token::Detach can update CancelRequested result
You should either use it only after the CancelRequested
call or just use consumer->CancelToken().CancelRequested instead

First approach saves some dynamic dispatching so it is preffered
*/

//////////////////////////////////////////////////////////////

struct SignalReceiver : public threads::lockfree::TaggedNode<SignalReceiver> {
  // Called by self or from above the chain
  virtual void Forward(Signal) = 0;

  virtual ~SignalReceiver() = default;
};

//////////////////////////////////////////////////////////////

struct SignalSender {
  // Called from below the chain
  virtual bool CancelRequested() = 0;

  virtual bool Cancellable() = 0;

  virtual void Attach(SignalReceiver*) = 0;

  virtual void Detach(SignalReceiver*) = 0;

  virtual ~SignalSender() = default;
};

//////////////////////////////////////////////////////////////

class Token {
 public:
  Token(const Token&) = default;
  Token& operator=(const Token&) = default;

  Token(Token&&) = default;
  Token& operator=(Token&&) = default;

  static Token Fabricate(SignalSender* source) {
    return Token(source);
  }

  bool CancelRequested() const {
    return owner_->CancelRequested();
  }

  bool Cancellable() const {
    return owner_->Cancellable();
  }

  void Attach(SignalReceiver* branch) {
    owner_->Attach(branch);
  }

  void Detach(SignalReceiver* branch) {
    owner_->Detach(branch);
  }

 private:
  explicit Token(SignalSender* source)
      : owner_{source} {
  }

 private:
  SignalSender* owner_;
};

//////////////////////////////////////////////////////////////

struct CancelledException {};

}  // namespace weave::cancel