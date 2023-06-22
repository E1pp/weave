#include <weave/executors/fibers/manual.hpp>
#include <weave/executors/submit.hpp>

#include <weave/fibers/core/fiber.hpp>

#include <weave/fibers/sched/yield.hpp>

#include <weave/futures/make/submit.hpp>

#include <wheels/test/framework.hpp>

using namespace weave; // NOLINT

TEST_SUITE(Manual) {
  SIMPLE_TEST(JustWorks) {
    executors::fibers::ManualExecutor manual;

    size_t step = 0;

    ASSERT_FALSE(manual.NonEmpty());

    ASSERT_FALSE(manual.RunNext());
    ASSERT_EQ(manual.RunAtMost(99), 0);

    executors::Submit(manual, [&] {
      fibers::Fiber* context = fibers::Fiber::Self();
      ASSERT_TRUE(context != nullptr);

      step = 1;
    });

    ASSERT_TRUE(manual.NonEmpty());
    ASSERT_EQ(manual.TaskCount(), 1);

    ASSERT_EQ(step, 0);

    executors::Submit(manual, [&] {
      fibers::Fiber* context = fibers::Fiber::Self();
      ASSERT_TRUE(context != nullptr);

      step = 2;
    });

    ASSERT_EQ(manual.TaskCount(), 2);

    ASSERT_EQ(step, 0);

    ASSERT_TRUE(manual.RunNext());

    ASSERT_EQ(step, 1);

    ASSERT_TRUE(manual.NonEmpty());
    ASSERT_EQ(manual.TaskCount(), 1);

    executors::Submit(manual, [&] {
      fibers::Fiber* context = fibers::Fiber::Self();
      ASSERT_TRUE(context != nullptr);

      step = 3;
    });

    ASSERT_EQ(manual.TaskCount(), 2);

    ASSERT_EQ(manual.RunAtMost(99), 2);
    ASSERT_EQ(step, 3);

    ASSERT_FALSE(manual.NonEmpty());
    ASSERT_FALSE(manual.RunNext());

    manual.Stop();
  }

  class Looper {
   public:
    explicit Looper(executors::IExecutor& e, size_t iters)
        : executor_(e),
          iters_left_(iters) {
    }

    void Start() {
      Submit();
    }

    void Iter() {
      --iters_left_;
      if (iters_left_ > 0) {
        Submit();
      }
    }

   private:
    void Submit() {
      executors::Submit(executor_, [this] {
        fibers::Fiber* context = fibers::Fiber::Self();
        ASSERT_TRUE(context != nullptr);

        Iter();
      });
    }

   private:
    executors::IExecutor& executor_;
    size_t iters_left_;
  };

  SIMPLE_TEST(RunAtMost) {
    executors::fibers::ManualExecutor manual;

    Looper looper{manual, 256};
    looper.Start();

    size_t tasks = 0;
    do {
      tasks += manual.RunAtMost(7);
    } while (manual.NonEmpty());

    ASSERT_EQ(tasks, 256);

    manual.Stop();
  }

  SIMPLE_TEST(Drain) {
    executors::fibers::ManualExecutor manual;

    Looper looper{manual, 117};
    looper.Start();

    ASSERT_EQ(manual.Drain(), 117);

    manual.Stop();
  }
}

///////////////////////////////////////////////////

TEST_SUITE(ManualSched){
  SIMPLE_TEST(Yield1) {
    executors::fibers::ManualExecutor scheduler;
    const size_t iters = 7;

    executors::Submit(scheduler, [&]{
      for(size_t i = 0; i < iters; i++){
        fibers::Yield();
      }
    });

    ASSERT_EQ(scheduler.Drain(), iters + 1);

    ASSERT_TRUE(scheduler.IsEmpty());

    executors::Submit(scheduler, [&]{
      for(size_t i = 0; i < iters; i++){
        fibers::Yield();
      }
    });

    ASSERT_EQ(scheduler.RunAtMost(iters / 2), iters / 2);

    ASSERT_TRUE(scheduler.NonEmpty());

    ASSERT_EQ(scheduler.RunNext(), 1);

    ASSERT_GE(scheduler.Drain(), 0);

    ASSERT_EQ(scheduler.Drain(), 0);

    scheduler.Stop();
  }

  SIMPLE_TEST(Yield2) {
    executors::fibers::ManualExecutor scheduler;

    bool done = false;

    executors::Submit(scheduler, [&] {
      fibers::Yield();
      done = true;
    });

    ASSERT_FALSE(done);

    scheduler.RunNext();
    ASSERT_FALSE(done);

    scheduler.RunNext();
    ASSERT_TRUE(done);

    ASSERT_FALSE(scheduler.RunNext());

    scheduler.Stop();
  }

  SIMPLE_TEST(Yield3) {
    executors::fibers::ManualExecutor scheduler;

    enum State : int {
      Ping = 0,
      Pong = 1
    };

    int state = Ping;

    executors::Submit(scheduler, [&] {
      for (size_t i = 0; i < 2; ++i) {
        ASSERT_EQ(state, Ping);
        state = Pong;

        fibers::Yield();
      }
    });

    executors::Submit(scheduler, [&] {
      for (size_t j = 0; j < 2; ++j) {
        ASSERT_EQ(state, Pong);
        state = Ping;

        fibers::Yield();
      }
    });

    size_t tasks = scheduler.Drain();

    ASSERT_EQ(tasks, 6);
    scheduler.Stop();
  }

  SIMPLE_TEST(HopAround){
    executors::fibers::ManualExecutor manual1;
    executors::fibers::ManualExecutor manual2;

    futures::Submit(manual1, [&](){
      fibers::Yield();
    }) | futures::Via(manual2)
       | futures::Map([&](Unit){
        fibers::Yield();
       })
       | futures::Detach();

    ASSERT_EQ(manual1.Drain(), 2); 
    ASSERT_EQ(manual2.Drain(), 2);
    ASSERT_EQ(manual1.TaskCount(), 0);

    manual1.Stop();
    manual2.Stop();
  }
}

RUN_ALL_TESTS()
