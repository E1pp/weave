#include <weave/satellite/logger.hpp>

#include <fmt/core.h>

#include <wheels/test/framework.hpp>

#include <thread>
#include <vector>

using namespace weave; // NOLINT

TEST_SUITE(Logger){
    SIMPLE_TEST(JustWorks){
        satellite::Logger<true, false> logger({"Test"}, 1);

        auto* logger_shard = logger.MakeShard(0);

        logger_shard->Increment("Test", 1);

        auto data = logger.GatherMetrics().Data();
        
        auto [name, count] = data[0];

        ASSERT_EQ(count, 1);
        ASSERT_EQ(name, "Test");
    }

    SIMPLE_TEST(Transparent){
        satellite::Logger<false, false> logger({"Test"}, 1);

        auto* logger_shard = logger.MakeShard(0);

        logger_shard->Increment("Test", 1);

        auto data = logger.GatherMetrics().Data();
        
        ASSERT_EQ(data, Unit{});
    }

    SIMPLE_TEST(TwoMetrics){
        satellite::Logger<true, false> logger({"Test1", "Test2"}, 1);

        auto* shard = logger.MakeShard(0);

        shard->Increment("Test1", 42);
        shard->Increment("Test2", 37);

        auto data = logger.GatherMetrics().Data();

        ASSERT_EQ(data.size(), 2);

        auto [s1, d1] = data[0];
        auto [s2, d2] = data[1];

        fmt::println("Order, in which metrics are returned,\ncan be different from the one, they been given");

        ASSERT_TRUE((s1 == "Test1" && s2 == "Test2") || (s1 == "Test2" && s2 == "Test1"));
        ASSERT_TRUE((s1 == "Test1" && d1 == 42 && d2 == 37) // NOLINT
                  || s1 == "Test2" && d1 == 37 && d2 == 42); 
    }

    SIMPLE_TEST(LogFromAnotherThread){
        satellite::Logger<true, false> logger({"Test"}, 1);
        
        std::thread async([&logger]{
            auto* shard = logger.MakeShard(0);

            shard->Increment("Test", 14);
        });

        async.join();

        auto [_, count] = logger.GatherMetrics().Data()[0];

        ASSERT_EQ(count, 14);
    }

    SIMPLE_TEST(ManyShards){
        const size_t k_num_shards = 3;

        satellite::Logger<true, false> logger({"Test"}, k_num_shards);

        std::vector<std::thread> threads{};

        for(size_t i = 0; i < k_num_shards; i++){
            threads.emplace_back([&logger, i]{
                auto* shard = logger.MakeShard(i);

                shard->Increment("Test", 1);
            });
        }

        for(auto& th : threads){
            th.join();
        }

        auto [_, count] = logger.GatherMetrics().Data()[0];

        ASSERT_EQ(count, k_num_shards);
    }

    SIMPLE_TEST(TwoShardsTwoMetrics){
        satellite::Logger<true, false> logger({"Test1", "Test2"}, 2);

        std::vector<std::thread> threads{};

        for(size_t i = 0; i < 2; i++){
            threads.emplace_back([&logger, i]{
                auto* shard = logger.MakeShard(i);

                std::string metric = i == 0 ? "Test1" : "Test2";
                shard->Increment(metric, 42 + i);
            });
        }

        for(auto& th : threads){
            th.join();
        }

        auto data = logger.GatherMetrics().Data();

        auto [_1, d1] = data[0];
        auto [_2, d2] = data[1];

        ASSERT_TRUE((d1 == 42 && d2 == 43) || (d1 == 43 && d2 == 42)); // NOLINT
    }
}

RUN_ALL_TESTS()