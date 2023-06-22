#pragma once

#include <weave/executors/task.hpp>

#include <twist/ed/stdlike/atomic.hpp>

#include <array>
#include <span>

namespace weave::executors::tp::fast {

template <size_t Capacity>
class WorkStealingQueue {
  struct Slot {
    Slot() = default;
    explicit Slot(Task* it)
        : item_(it) {
    }

    Slot(const Slot&) = default;
    Slot& operator=(const Slot&) = default;

    Slot(Slot&&) = default;
    Slot& operator=(Slot&&) = default;

    twist::ed::stdlike::atomic<Task*> item_{nullptr};
  };

 public:
  bool TryPush(Task* item) {
    size_t cached_head = head_.load(std::memory_order::acquire);
    size_t cached_tail =
        tail_.load(std::memory_order::relaxed);  // sync done via PO

    // check if buffer is full
    if (MakeValid(cached_tail - cached_head + 1) == 0) {
      return false;
    }

    // write into buffer
    buffer_[MakeValid(cached_tail)].item_.store(item,
                                                std::memory_order::relaxed);

    // advance tail_
    tail_.store(cached_tail + 1,
                std::memory_order::release);  // make tail visible to everyone

    return true;
  }

  // For grabbing from global queue / for stealing
  // Should always succeed
  void PushMany(std::span<Task*> buffer) {
    // success cause used only when queue is empty

    for (auto task : buffer) {
      TryPush(task);
    }
  }

  // Returns nullptr if queue is empty
  Task* TryPop() {
    // This function can be called only by the thread which is allowed to call
    // TryPush. this means effectively that while we are in this call, tail will
    // remain unchanged.
    size_t cached_tail = tail_.load(
        std::memory_order::relaxed);  // sync with producer done via PO
    size_t cached_head = head_.load(std::memory_order::relaxed);

    Task* ret_val;

    do {
      // if we observe an empty buffer at any point we quit
      if (cached_tail == cached_head) {
        return nullptr;
      }
      ret_val = buffer_[MakeValid(cached_head)].item_.load(
          std::memory_order::relaxed);

      // we check if head_ hasn't changed and if it didn't we advance it right
      // away
    } while (!head_.compare_exchange_weak(cached_head, cached_head + 1,
                                          std::memory_order::relaxed));

    // since we are the only caller of TryPush it cannot intercept here
    // so no need to save value before advancing

    assert(ret_val != nullptr);
    return ret_val;
  }

  // For stealing and for offloading to global queue
  // Returns number of tasks in `out_buffer`
  size_t Grab(std::span<Task*> out_buffer) {
    const size_t buff_size = out_buffer.size();
    if (buff_size == 0) {
      return 0;
    }

    size_t num_grabbed;

    size_t cached_head = head_.load(std::memory_order::relaxed);
    size_t cached_tail;

    do {
      cached_tail =
          tail_.load(std::memory_order::acquire);  // sync with producer
      // check how much you can grab

      if (cached_tail - cached_head == 0) {  // empty state observed
        return 0;
      }

      // experimental:
      // limit num_grabbed by half of total estimated size
      size_t threshold = std::max<size_t>((cached_tail - cached_head) / 2, 1);

      num_grabbed = std::min(buff_size, threshold);

      // save stolen subarray
      WriteFromBuffer(out_buffer, cached_head, num_grabbed);

      // chech if head hasn't changed. If so, advance it by num_grabbed
    } while (!head_.compare_exchange_weak(
        cached_head, cached_head + num_grabbed, std::memory_order::release,
        std::memory_order::relaxed));

    return num_grabbed;
  }

  size_t SizeEstimate() {
    size_t head_estimate = head_.load(std::memory_order::relaxed);
    size_t tail_estimate = tail_.load(std::memory_order::relaxed);
    return tail_estimate - head_estimate;
  }

 private:
  size_t MakeValid(size_t index) {
    return index % buffer_.max_size();
  }

  void WriteFromBuffer(std::span<Task*> out_buffer, size_t beg,
                       size_t num_elements) {
    auto iter = out_buffer.begin();
    for (size_t i = beg; i < beg + num_elements; i++) {
      *iter = buffer_[MakeValid(i)].item_.load(std::memory_order::relaxed);
      iter++;
    }
  }

 private:
  twist::ed::stdlike::atomic<size_t> head_{0};
  twist::ed::stdlike::atomic<size_t> tail_{0};

  std::array<Slot, Capacity + 1> buffer_{};
};

}  // namespace weave::executors::tp::fast
