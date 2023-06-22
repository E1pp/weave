#include <weave/executors/thread_pool.hpp>
#include <weave/executors/strand.hpp>
#include <weave/executors/submit.hpp>

#include <weave/threads/lockfull/wait_group.hpp>

#include <twist/test/with/wheels/stress.hpp>

#include <twist/test/repeat.hpp>

#include <twist/ed/stdlike/thread.hpp>

#include <twist/ed/stdlike/atomic.hpp>

using namespace weave; // NOLINT
using namespace std::chrono_literals;

/////////////////////////////////////////////////////////////////////

class OnePassBarrier {
 public:
  explicit OnePassBarrier(size_t threads) : total_(threads) {
  }

  void Pass() {
    arrived_.fetch_add(1);
    while (arrived_.load() < total_) {
      twist::ed::stdlike::this_thread::yield();
    }
  }

 private:
  const size_t total_{0};
  twist::ed::stdlike::atomic<size_t> arrived_{0}; //NOLINT
};

void MaybeAnomaly() {
  executors::ThreadPool pool{1};
  pool.Start();

  twist::test::Repeat repeat;

  while (repeat()) {
    executors::Strand strand{pool};
    OnePassBarrier barrier{2};

    threads::lockfull::WaitGroup wg;
    wg.Add(2);

    executors::Submit(strand, [&wg, &barrier] {
      wg.Done();
      barrier.Pass();
    });

    barrier.Pass();

    executors::Submit(strand, [&wg] {
      wg.Done();
    });

    wg.Wait();
  }

  pool.Stop();
}

//////////////////////////////////////////////////////////////////////

#if defined(NDEBUG)

TEST_SUITE(Strand) {
  TWIST_TEST(MemoryOrder, 5s) {
    MaybeAnomaly();
  }
}

#endif

RUN_ALL_TESTS()
