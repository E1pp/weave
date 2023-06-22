#include <weave/threads/lockfull/stdlike/mutex.hpp>

namespace weave::threads::lockfull::stdlike {

void Mutex::Lock() {
  if (CompareExchange(0, 1) != 0) {
    do {
      CompareExchange(1, 2);
      twist::ed::Wait(state_, 2);

    } while (!TryLock());
  }
}
void Mutex::Unlock() {
  auto wake_key = twist::ed::PrepareWake(state_);
  if (state_.exchange(0) == 2) {
    twist::ed::WakeOne(wake_key);
  }
}

}  // namespace weave::threads::lockfull::stdlike