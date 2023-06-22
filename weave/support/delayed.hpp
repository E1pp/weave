#pragma once

#include <memory>
#include <type_traits>
#include <utility>

namespace weave::support {

template <typename T>
class Delayed {
 public:
  Delayed() = default;

  // Non-copyable
  Delayed(const Delayed&) = delete;
  Delayed& operator=(const Delayed&) = delete;

  // Movable
  Delayed(Delayed&&) = default;
  Delayed& operator=(Delayed&&) = default;

  // Create an actual object
  template <typename... Args>
  requires std::is_constructible_v<T, Args...>
  void Create(Args&&... args) {
    ::new (&object_) T(std::forward<Args>(args)...);
  }

  T& Refer() {
    return *std::launder(reinterpret_cast<T*>(&object_));
  }

  void Destroy() {
    std::destroy_at(std::launder(reinterpret_cast<T*>(&object_)));
  }

  ~Delayed() = default;

 private:
  alignas(T) std::byte object_[sizeof(T)];
};

///////////////////////////////////////////////////////////////////////////////

template <bool IsDelayed, typename T>
class MaybeDelayed {
 public:
  MaybeDelayed() = default;

  template <typename... Args>
  requires std::is_constructible_v<T, Args...>
  explicit MaybeDelayed(Args&&...)
      : MaybeDelayed() {
    // No-Op
  }

  // Non-copyable
  MaybeDelayed(const MaybeDelayed&) = delete;
  MaybeDelayed& operator=(const MaybeDelayed&) = delete;

  // Movable
  MaybeDelayed(MaybeDelayed&&) = default;
  MaybeDelayed& operator=(MaybeDelayed&&) = default;

  template <typename... Args>
  requires std::is_constructible_v<T, Args...>
  void Create(Args&&... args) {
    delayed_.Create(std::forward<Args>(args)...);
  }

  T& Refer() {
    return delayed_.Refer();
  }

  void Destroy() {
    delayed_.Destroy();
  }

 private:
  Delayed<T> delayed_;
};

template <typename T>
class MaybeDelayed<false, T> {
 public:
  template <typename... Args>
  requires std::is_constructible_v<T, Args...>
  explicit MaybeDelayed(Args&&... args)
      : object_(std::forward<Args>(args)...) {
  }

  template <typename... Args>
  requires std::is_constructible_v<T, Args...>
  void Create(Args&&...) {
    // No-Op
  }

  T& Refer() {
    return object_;
  }

  void Destroy() {
    // No-Op
  }

 private:
  T object_;
};

};  // namespace weave::support