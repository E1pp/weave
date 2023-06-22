#pragma once

#include <weave/fibers/sync/experimental/lf_chan/detail/waiters.hpp>

#include <twist/ed/stdlike/atomic.hpp>

#include <array>
#include <limits>

namespace weave::fibers::experimental::detail {

#if !defined(TWIST_FAULTY)
constexpr inline size_t kSegmentSize = 256;
#else
constexpr inline size_t kSegmentSize = 16;
#endif

template <typename T>
inline constexpr IWaiter<T>* kFullptr =
    reinterpret_cast<IWaiter<T>*>(std::numeric_limits<uintptr_t>::max());

template <typename T>
struct Segment {
  explicit Segment(size_t id)
      : id_(id) {
  }

  struct Slot {
    twist::ed::stdlike::atomic<State> state_{State::Null};
    twist::ed::stdlike::atomic<IWaiter<T>*> waiter_{nullptr};
  };

  size_t id_;
  std::array<Slot, kSegmentSize> array_{};
  twist::ed::stdlike::atomic<Segment<T>*> next_{nullptr};
};

}  // namespace weave::fibers::experimental::detail