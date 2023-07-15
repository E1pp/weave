#include <wheels/test/framework.hpp>

#include <twist/test/with/wheels/stress.hpp>
#include <twist/test/repeat.hpp>
#include <twist/test/race.hpp>

#include <weave/executors/thread_pool.hpp>

#include <weave/futures/make/contract.hpp>
#include <weave/futures/make/submit.hpp>

#include <weave/futures/combine/seq/map.hpp>
#include <weave/futures/combine/seq/flat_map.hpp>
#include <weave/futures/combine/seq/and_then.hpp>
#include <weave/futures/combine/seq/or_else.hpp>
#include <weave/futures/combine/seq/via.hpp>
#include <weave/futures/combine/seq/box.hpp>

#include <weave/futures/combine/par/all.hpp>
#include <weave/futures/combine/par/first.hpp>
#include <weave/futures/combine/par/quorum.hpp>

#include <weave/futures/run/get.hpp>
#include <weave/futures/run/detach.hpp>

#include <twist/ed/stdlike/thread.hpp>

#include <wheels/core/defer.hpp>

#include <fmt/core.h>

#include <atomic>
#include <chrono>

using namespace weave; // NOLINT
using namespace std::chrono_literals;

//////////////////////////////////////////////////////////////////////

inline std::error_code TimeoutError() {
  return std::make_error_code(std::errc::timed_out);
}

//////////////////////////////////////////////////////////////////////

void StressTestContract() {
  twist::test::Repeat repeat;

  while (repeat()) {
    auto [f, p] = futures::Contract<int>();

    int iter = repeat.Iter();

    twist::ed::stdlike::thread producer([p = std::move(p), iter]() mutable {
      std::move(p).SetValue(iter);
    });

    twist::ed::stdlike::thread consumer([f = std::move(f), iter]() mutable {
      auto r = std::move(f) | futures::Get();
      ASSERT_TRUE(r);
      ASSERT_EQ(*r, iter);
    });

    producer.join();
    consumer.join();
  }

  fmt::println("Iterations: {}", repeat.IterCount());
}

//////////////////////////////////////////////////////////////////////

void StressTestPipeline() {
  executors::ThreadPool pool{4};

  pool.Start();

  twist::test::Repeat repeat;

  while (repeat()) {
    size_t pipelines = 1 + repeat.Iter() % 3;

    std::atomic<size_t> counter1{0};
    std::atomic<size_t> counter2{0};
    std::atomic<size_t> counter3{0};

    std::vector<futures::BoxedFuture<Unit>> futs;

    for (size_t j = 0; j < pipelines; ++j) {
      auto f = futures::Submit(pool,
                      [&]() {
                        ++counter1;
                        return result::Ok();
                      })
          | futures::Via(pool)
          | futures::Map([&](Unit) -> Unit {
              ++counter2;
              return {};
            })
          | futures::Map([&](Unit) -> Unit {
              ++counter3;
              return {};
            });

      futs.push_back(std::move(f));
    }

    for (auto&& f : futs) {
      std::move(f) | futures::Get();
    }

    ASSERT_EQ(counter1.load(), pipelines);
    ASSERT_EQ(counter2.load(), pipelines);
    ASSERT_EQ(counter3.load(), pipelines);
  }

  fmt::println("Iterations: {}", repeat.IterCount());

  pool.Stop();
}

//////////////////////////////////////////////////////////////////////

void StressTestFirst() {
  executors::tp::fast::ThreadPool pool{4};
  pool.Start();

  twist::test::Repeat repeat;

  while (repeat()) {
    size_t i = repeat.Iter();

    auto f = futures::Submit(pool, [&, i]() -> Result<int> {
      if (i % 3 == 0) {
        return result::Err(TimeoutError());
      } else {
        return result::Ok(1);
      }
    });

    auto g = futures::Submit(pool, [&, i]() -> Result<int> {
      if (i % 4 == 0) {
        return result::Err(TimeoutError());
      } else {
        return result::Ok(2);
      }
    });

    auto first = futures::no_alloc::First(std::move(f), std::move(g));

    auto r = std::move(first) | futures::Get();

    if (i % 12 != 0) {
      ASSERT_TRUE(r);
      ASSERT_TRUE((*r == 1) || (*r == 2)); // NOLINT
    } else {
      ASSERT_FALSE(r);
    }

    pool.WaitIdle();
  }

  fmt::println("Iterations: {}", repeat.IterCount());

  pool.Stop();
}

//////////////////////////////////////////////////////////////////////

void StressTestAll() {
  executors::ThreadPool pool{4};
  pool.Start();

  twist::test::Repeat repeat;

  while (repeat()) {
    size_t i = repeat.Iter();

    auto f = futures::Submit(pool, [i]() -> Result<int> {
      if (i % 7 == 5) {
        return result::Err(TimeoutError());
      } else {
        return result::Ok(1);
      }
    });

    auto g = futures::Submit(pool, [i]() -> Result<int> {
      if (i % 7 == 6) {
        return result::Err(TimeoutError());
      } else {
        return result::Ok(2);
      }
    });

    auto both = futures::no_alloc::All(std::move(f), std::move(g));

    auto r = std::move(both) | futures::Get();

    if (i % 7 < 5) {
      auto [x, y] = *r;
      ASSERT_EQ(x, 1);
      ASSERT_EQ(y, 2);
    } else {
      ASSERT_FALSE(r);
    }
  }

  fmt::println("Iterations: {}", repeat.IterCount());

  pool.Stop();
}

//////////////////////////////////////////////////////////////////////

void StressTestQuorumFirst() {
  executors::tp::fast::ThreadPool pool{4};
  pool.Start();

  twist::test::Repeat repeat;

  while (repeat()) {
    size_t i = repeat.Iter();

    auto f = futures::Submit(pool, [&, i]() -> Result<int> {
      if (i % 3 == 0) {
        return result::Err(TimeoutError());
      } else {
        return result::Ok(1);
      }
    });

    auto g = futures::Submit(pool, [&, i]() -> Result<int> {
      if (i % 4 == 0) {
        return result::Err(TimeoutError());
      } else {
        return result::Ok(2);
      }
    });

    auto first = futures::no_alloc::Quorum(1, std::move(f), std::move(g));

    auto r = std::move(first) | futures::Get();

    if (i % 12 != 0) {
      ASSERT_TRUE(r);
      ASSERT_TRUE(((*r)[0] == 1) || ((*r)[0] == 2)); // NOLINT
    } else {
      ASSERT_FALSE(r);
    }

    pool.WaitIdle();
  }

  fmt::println("Iterations: {}", repeat.IterCount());

  pool.Stop();
}

//////////////////////////////////////////////////////////////////////

void StressTestQuorumAll() {
  executors::ThreadPool pool{4};
  pool.Start();

  twist::test::Repeat repeat;

  while (repeat()) {
    size_t i = repeat.Iter();

    auto f = futures::Submit(pool, [i]() -> Result<int> {
      if (i % 7 == 5) {
        return result::Err(TimeoutError());
      } else {
        return result::Ok(1);
      }
    });

    auto g = futures::Submit(pool, [i]() -> Result<int> {
      if (i % 7 == 6) {
        return result::Err(TimeoutError());
      } else {
        return result::Ok(2);
      }
    });

    auto both = futures::no_alloc::Quorum(2, std::move(f), std::move(g));

    auto r = std::move(both) | futures::Get();

    if (i % 7 < 5) {
      int x = (*r)[0];
      int y = (*r)[1];

      ASSERT_TRUE((x == 1 && y == 2) || (x == 2 && y == 1)); // NOLINT
    } else {
      ASSERT_FALSE(r);
    }
  }

  fmt::println("Iterations: {}", repeat.IterCount());

  pool.Stop();
}

//////////////////////////////////////////////////////////////////////

TEST_SUITE(Futures) {
  TWIST_TEST(StressContract, 5s) {
    StressTestContract();
  }

  TWIST_TEST(StressPipeline, 5s) {
    StressTestPipeline();
  }

  TWIST_TEST(StressFirst, 5s) {
    StressTestFirst();
  }
  
  TWIST_TEST(StressBoth, 5s) {
    StressTestAll();
  }

  TWIST_TEST(StressQuorumFirst, 5s){
    StressTestQuorumFirst();
  }

  TWIST_TEST(StressQuorumAll, 5s){
    StressTestQuorumAll();
  }
}

RUN_ALL_TESTS()
