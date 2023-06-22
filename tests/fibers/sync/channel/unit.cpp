#include <weave/executors/thread_pool.hpp>
#include <weave/executors/manual.hpp>

#include <weave/fibers/sched/go.hpp>
#include <weave/fibers/sched/yield.hpp>

#include <weave/fibers/sync/wait_group.hpp>
#include <weave/fibers/sync/channel.hpp>

#include <weave/threads/lockfull/wait_group.hpp>

#include <wheels/test/framework.hpp>

#include <twist/rt/run.hpp>

#include <chrono>
#include <thread>

#include <fmt/core.h>

using namespace weave; // NOLINT

using namespace std::chrono_literals;

//////////////////////////////////////////////////////////////////////

struct MoveOnly {
  MoveOnly(std::string data) : data_(data) { // NOLINT
  }

  MoveOnly(const MoveOnly& that) = delete;
  MoveOnly& operator=(const MoveOnly& that) = delete;

  MoveOnly(MoveOnly&& that) = default;

  std::string Data(){
    return data_;
  }

  std::string data_;
};

//////////////////////////////////////////////////////////////////////

TEST_SUITE(Channels) {
  SIMPLE_TEST(JustWorks) {
    executors::ManualExecutor manual;

    bool done = false;

    fibers::Go(manual, [&done]() {
      fibers::Channel<int> ints{7};

      ints.Send(1);
      ints.Send(2);
      ints.Send(3);

      ASSERT_EQ(ints.Receive(), 1);
      ASSERT_EQ(ints.Receive(), 2);
      ASSERT_EQ(ints.Receive(), 3);

      done = true;
    });

    size_t tasks = manual.Drain();

    ASSERT_EQ(tasks, 1);
    ASSERT_TRUE(done);
  }

  SIMPLE_TEST(MoveOnly) {
    executors::ManualExecutor manual;

    fibers::Go(manual, []() {
      fibers::Channel<MoveOnly> objs{4};

      objs.Send({"Hello"});
      objs.Send({"World"});

      {
        auto obj = objs.Receive();
        ASSERT_EQ(obj.Data(), "Hello");
      }

      {
        auto obj = objs.Receive();
        ASSERT_EQ(obj.Data(), "World");
      }
    });

    manual.Drain();
  }

  SIMPLE_TEST(TryReceive) {
    executors::ManualExecutor manual;

    fibers::Go(manual, []() {
      fibers::Channel<int> ints{/*buffer=*/1};

      ASSERT_FALSE(ints.TryReceive());

      ints.Send(14);

      {
        auto value = ints.TryReceive();
        ASSERT_TRUE(value);
        ASSERT_EQ(*value, 14);
      }

      ASSERT_FALSE(ints.TryReceive());
    });

    manual.Drain();
  }

  SIMPLE_TEST(TrySend) {
    executors::ManualExecutor manual;

    fibers::Go(manual, []() {
      fibers::Channel<int> ints{/*buffer=*/1};

      ints.Send(1);
      ASSERT_FALSE(ints.TrySend(2));
      ints.Receive();
      ASSERT_TRUE(ints.TrySend(2));
      ASSERT_EQ(ints.Receive(), 2);
    });

    manual.Drain();
  }

  SIMPLE_TEST(SuspendReceiver) {
    executors::ManualExecutor manual;

    fibers::Channel<int> ints{3};

    bool done = false;

    // Receiver
    fibers::Go(manual, [&done, ints]() mutable {
      int value = ints.Receive();  // <-- Suspended
      ASSERT_EQ(value, 17);
      done = true;
    });

    {
      size_t tasks = manual.Drain();
      ASSERT_TRUE(tasks < 3);
      ASSERT_FALSE(done);
    }

    // Sender
    fibers::Go(manual, [ints]() mutable {
      ints.Send(17);  // <-- Resume suspended receiver
    });

    manual.Drain();

    ASSERT_TRUE(done);
  }

  SIMPLE_TEST(SuspendSender) {
    executors::ManualExecutor manual;

    fibers::Channel<int> ints{2};

    int step = 0;

    // Sender
    fibers::Go(manual, [&step, ints]() mutable {
      ints.Send(1);
      step = 1;
      ints.Send(2);
      step = 2;
      ints.Send(3);  // <-- Suspended
      step = 3;
    });

    {
      size_t tasks = manual.Drain();
      ASSERT_TRUE(tasks < 5);
      ASSERT_EQ(step, 2);
    }

    bool all_received = false;

    // Receiver
    fibers::Go(manual, [&all_received, ints]() mutable {
      {
        int value = ints.Receive();  // <-- Resume suspended sender
        ASSERT_EQ(value, 1);
      }

      fibers::Yield();

      ASSERT_EQ(ints.Receive(), 2);
      ASSERT_EQ(ints.Receive(), 3);

      all_received = true;
    });

    // Receive + Complete sender
    manual.RunAtMost(2);

    ASSERT_EQ(step, 3);

    // Complete receiver
    manual.Drain();

    ASSERT_TRUE(all_received);
  }

  SIMPLE_TEST(Fifo) {
    executors::ManualExecutor manual;

    fibers::Channel<int> ints{7};

    static const int kMessages = 128;

    fibers::Go(manual, [ints]() mutable {
      for (int i = 0; i < kMessages; ++i) {
        ints.Send(i);

        if (i % 3 == 0) {
          fibers::Yield();
        }
      }
    });

    bool done = false;

    fibers::Go(manual, [&done, ints]() mutable {
      for (int j = 0; j < kMessages; ++j) {
        ASSERT_EQ(ints.Receive(), j);

        if (j % 2 == 0) {
          fibers::Yield();
        }
      }
      done = true;
    });

    {
      size_t tasks = manual.Drain();
      ASSERT_TRUE(tasks < 512);
    }

    ASSERT_TRUE(done);
  }

  SIMPLE_TEST(Concurrent) {
    executors::ThreadPool scheduler{4};

    scheduler.Start();

    threads::lockfull::WaitGroup wg;

    fibers::Channel<int> ints{11};

    static const size_t kSenders = 17;
    static const size_t kReceivers = 19;

    std::atomic<size_t> senders{kSenders};

    std::atomic<size_t> produced{0};
    std::atomic<size_t> consumed{0};

    wg.Add(kSenders + kReceivers);

    for (size_t i = 0; i < kSenders; ++i) {
      fibers::Go(scheduler, [&, ints]() mutable {
        for (int k = 0; k < 512; ++k) {
          ints.Send(k);
          produced += k;
        }

        if (senders.fetch_sub(1) == 1) {
          // Last sender, send poison pills
          for (size_t j = 0; j < kReceivers; ++j) {
            ints.Send(-1);
          }
        }

        wg.Done();
      });
    }

    for (size_t j = 0; j < kReceivers; ++j) {
      fibers::Go(scheduler, [&wg, &consumed, ints]() mutable {
        while (true) {
          int value = ints.Receive();
          if (value == -1) {
            break;
          }
          consumed += value;
        }

        wg.Done();
      });
    }

    wg.Wait();

    ASSERT_EQ(produced.load(), consumed.load());

    scheduler.Stop();
  }

  SIMPLE_TEST(FairnessExample) {
    executors::ThreadPool scheduler{1};

    scheduler.Start();

    threads::lockfull::WaitGroup outer;

    outer.Add(1);

    fibers::Go(scheduler, [&outer]() {
      fibers::Channel<int> xs{1};
      fibers::Channel<int> ys{1};

      fibers::WaitGroup wg;
      wg.Add(3);

      std::atomic<bool> stop_requested{false};

      fibers::Go([&]() {
        while (!stop_requested.load()) {
          ys.Receive();
          // std::cout << "Ping" << std::endl;
          xs.Send(1);
        }
        wg.Done();
      });

      fibers::Go([&]() {
        ys.Send(1);
        while (!stop_requested.load()) {
          xs.Receive();
          // std::cout << "Pong" << std::endl;
          ys.Send(1);
        }
        wg.Done();
      });

      fibers::Go([&]() {
        for (size_t i = 0; i < 10; ++i) {
          fibers::Yield();
        }


        stop_requested.store(true);
        wg.Done();
      });

      wg.Wait();
      outer.Done();
    });

    outer.Wait();
    scheduler.Stop();
  }
}

RUN_ALL_TESTS()
