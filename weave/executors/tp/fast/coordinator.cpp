#include <weave/executors/tp/fast/coordinator.hpp>
#include <weave/executors/tp/fast/thread_pool.hpp>
#include <weave/executors/tp/fast/worker.hpp>

#include <twist/ed/wait/futex.hpp>

namespace weave::executors::tp::fast {

// TODO: memory orders!!

//////////////////////////////////////////////////////////////////////

struct StateView {
  uint32_t idle;
  uint32_t spinning;
};

StateView View(uint64_t state) {
  return {(uint32_t)(state >> 32),
          (uint32_t)(state & (((uint64_t)1 << 32) - 1))};
}

uint64_t AddSpinner(uint64_t state) {
  return state + 1;
}

static const uint64_t kIdleOffset = (uint64_t)1 << 32;

void AddIdle(twist::ed::stdlike::atomic<uint64_t>& state) {
  state.fetch_add(kIdleOffset);
}

void RemoveIdle(twist::ed::stdlike::atomic<uint64_t>& state) {
  state.fetch_sub(kIdleOffset);
}

//////////////////////////////////////////////////////////////////////

bool Coordinator::TryStartSpinning() {
  auto caller = Worker::Current();
  WHEELS_VERIFY(caller != nullptr,
                "TryStartSpinning: you can't be a non-worker!");

  uint64_t curr = state_.load();
  const size_t max_threads = caller->host_.threads_;

  while (true) {
    auto [idle, spinning] = View(curr);

    if (2 * spinning > max_threads) {
      caller->logger_shard_->Increment("Times denied by coordinator", 1);

      return false;
    }

    uint64_t next = AddSpinner(curr);

    if (state_.compare_exchange_weak(curr, next)) {
      return true;
    }
  }

  // we started spinning
  return true;
}

bool Coordinator::StopSpinning() {
  auto caller = Worker::Current();
  WHEELS_VERIFY(caller != nullptr, "StopSpinning: you can't be a non-worker!");

  uint64_t observed = state_.fetch_sub(1);

  auto [idle, spinning] = View(observed);

  return spinning == 1;
}

void Coordinator::TryParkMe(uint32_t old_wakeups) {
  auto caller = Worker::Current();
  WHEELS_VERIFY(caller != nullptr, "TryParkMe: you can't be a non-worker!");

  caller->logger_shard_->Increment("Syscal parkings", 1);

  twist::ed::futex::Wait(caller->wakeups_, old_wakeups,
                         std::memory_order::relaxed);

  CancelPark();
}

bool Coordinator::TryWakeWorker() {
  if (!ShouldWake()) {
    return true;
  }

  if (Worker* sleeper = sleepers_.TryDequeue()) {
    return sleeper->TryWake();
  }

  return true;
}

uint32_t Coordinator::AnnouncePark() {
  Worker* caller = Worker::Current();
  WHEELS_ASSERT(caller != nullptr, "AnnouncePark: you can't be a non-worker!");

  uint32_t ret_val = caller->wakeups_.load(std::memory_order::relaxed);

  caller->idle_.store(true, std::memory_order::relaxed);
  sleepers_.Enqueue(caller);
  AddIdle(state_);

  return ret_val;
}

void Coordinator::CancelPark() {
  Worker* caller = Worker::Current();
  WHEELS_ASSERT(caller != nullptr, "CancelPark: you can't be a non-worker!");

  RemoveIdle(state_);
  sleepers_.RemoveFromQueue(caller);
  caller->idle_.store(false);
}

bool Coordinator::ShouldWake() {
  uint64_t observed = state_.load();
  auto [idle, spinning] = View(observed);

  return idle > 0 && spinning == 0;
}

}  // namespace weave::executors::tp::fast
