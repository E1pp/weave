#include <weave/fibers/sched/sleep_for.hpp>

#include <weave/futures/make/after.hpp>

#include <weave/futures/run/await.hpp>

#include <weave/satellite/satellite.hpp>

namespace weave::fibers {

void SleepFor(timers::Millis delay) {
  satellite::PollToken();

  futures::After(delay) | futures::Await();
}

void SleepFor(timers::Delay delay) {
  satellite::PollToken();

  futures::After(delay) | futures::Await();
}

}  // namespace weave::fibers