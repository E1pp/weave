#include <weave/executors/tp/compute/thread_pool.hpp>
#include <weave/executors/strand.hpp>
#include <weave/executors/submit.hpp>

#include <weave/threads/blocking/wait_group.hpp>

#include <twist/test/with/wheels/stress.hpp>

#include <twist/test/repeat.hpp>

#include <list>

using namespace weave; // NOLINT
using namespace std::chrono_literals;

/////////////////////////////////////////////////////////////////////

void ConcurrentStrands(size_t mutexes, size_t pushes) {
  executors::tp::compute::ThreadPool workers{4};
  workers.Start();

  std::list<executors::Strand> strands;
  for (size_t i = 0; i < mutexes; ++i) {
    strands.emplace_back(workers);
  }

  executors::tp::compute::ThreadPool clients{mutexes};
  clients.Start();

  for (twist::test::Repeat repeat; repeat(); ) {
    threads::blocking::WaitGroup wg;
    wg.Add(mutexes * pushes);

    for (auto& strand : strands) {
      Submit(clients, [&strand, &wg, pushes] {
        for (size_t j = 0; j < pushes; ++j) {
          Submit(strand, [&wg] {
            wg.Done();
          });
        }
      });
    }

    wg.Wait();
  }

  clients.Stop();
  workers.Stop();
}

//////////////////////////////////////////////////////////////////////

void MissingTasks() {
  executors::tp::compute::ThreadPool pool{4};
  pool.Start();

  executors::Strand strand{pool};

  for (twist::test::Repeat repeat; repeat(); ) {
    const size_t todo = 2 + repeat.Iter() % 5;

    threads::blocking::WaitGroup wg;
    wg.Add(todo);

    for (size_t i = 0; i < todo; ++i) {
      executors::Submit(strand, [&wg] {
        wg.Done();
      });
    }

    wg.Wait();
  }

  pool.Stop();
}

//////////////////////////////////////////////////////////////////////

TEST_SUITE(Strand) {
  TWIST_TEST(ConcurrentStrands_1, 5s) {
    ConcurrentStrands(30, 30);
  }

  TWIST_TEST(ConcurrentStrands_2, 5s) {
    ConcurrentStrands(50, 20);
  }

  TWIST_TEST(MissingTasks, 5s) {
    MissingTasks();
  }
}

RUN_ALL_TESTS()
