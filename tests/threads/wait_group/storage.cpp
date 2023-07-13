#include <weave/threads/blocking/wait_group.hpp>

#include <twist/test/with/wheels/stress.hpp>

#include <twist/ed/stdlike/thread.hpp>

using weave::threads::blocking::WaitGroup;

//////////////////////////////////////////////////////////////////////

void StorageTest() {
  // Help AddressSanitizer
  auto* wg = new WaitGroup{};

  wg->Add(1);
  twist::ed::stdlike::thread t([wg] {
    wg->Done();
  });

  wg->Wait();
  delete wg;

  t.join();
}

//////////////////////////////////////////////////////////////////////

#if defined(TWIST_FIBERS)

TEST_SUITE(WaitGroup) {
  TWIST_TEST_REPEAT(Storage, 5s) {
    StorageTest();
  }
}

#endif

RUN_ALL_TESTS()
