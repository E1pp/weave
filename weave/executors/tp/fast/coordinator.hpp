#pragma once

#include <twist/ed/stdlike/atomic.hpp>

#include <weave/executors/tp/fast/queues/parking_lot.hpp>
#include <weave/threads/blocking/spinlock.hpp>
#include <weave/threads/blocking/stdlike/mutex.hpp>

namespace weave::executors::tp::fast {

// Coordinates workers (stealing, parking)

class Worker;

class Coordinator {
 public:
  // true iff we started successfully
  bool TryStartSpinning();

  // returns true if you were the last one spinning
  bool StopSpinning();

  uint32_t AnnouncePark();

  void CancelPark();

  void TryParkMe(uint32_t);

  bool ShouldWake();

  // true if no need for further backoff
  bool TryWakeWorker();

 private:
  twist::ed::stdlike::atomic<uint64_t> state_{0};
  ParkingLot<Worker> sleepers_;
};

}  // namespace weave::executors::tp::fast
