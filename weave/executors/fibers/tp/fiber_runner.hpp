#pragma once

#include <weave/fibers/core/fwd.hpp>

#include <weave/executors/tp/fast/runner.hpp>

#include <weave/executors/executor.hpp>

#include <wheels/intrusive/list.hpp>

namespace weave::executors::runners {

class FiberRunner : public IRunner {
  using Fiber = weave::fibers::Fiber;

 public:
  void RunnerRoutine(IPicker& picker) override;

  ~FiberRunner() override = default;
};

/////////////////////////////////////////////////////////////////////

class FiberRunnerShard {
  using Fiber = weave::fibers::Fiber;

 public:
  explicit FiberRunnerShard(IPicker* picker)
      : picker_(picker) {
  }

  void Run();

  void Stop();

  static FiberRunnerShard* CurrentShard();

  void RetireAsCarrier(Fiber* carrier);

 private:
  Fiber* GetCarrier();

  void CarrierRoutine();

  bool StopRequested() {
    return stopped_;
  }

 private:
  bool stopped_{false};
  IPicker* picker_;
  wheels::IntrusiveList<Fiber> pool_{};
};

}  // namespace weave::executors::runners