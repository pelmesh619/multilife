#include <gtest/gtest.h>
#include "ResourceManager.hpp"
#include <thread>
#include <vector>

using namespace multilife;

// Initial state test
TEST(ResourceManagerTest, NewPlayerBalanceIsZero) {
    ResourceManager rm;
    EXPECT_EQ(rm.getBalance(1), 0u);
}

// awardFromLiveCounts tests
TEST(ResourceManagerTest, AwardFromLiveCountsIncreasesBalance) {
    ResourceManager rm;
    rm.awardFromLiveCounts({{1, 100}});
    EXPECT_EQ(rm.getBalance(1), 100u);
}

TEST(ResourceManagerTest, AwardFromLiveCountsAccumulates) {
    ResourceManager rm;
    rm.awardFromLiveCounts({{1, 50}});
    rm.awardFromLiveCounts({{1, 30}});
    EXPECT_EQ(rm.getBalance(1), 80u);
}

TEST(ResourceManagerTest, AwardFromLiveCountsUpdatesMultiplePlayers) {
    ResourceManager rm;
    rm.awardFromLiveCounts({{1, 10}, {2, 20}, {3, 30}});
    EXPECT_EQ(rm.getBalance(1), 10u);
    EXPECT_EQ(rm.getBalance(2), 20u);
    EXPECT_EQ(rm.getBalance(3), 30u);
}

TEST(ResourceManagerTest, AwardFromLiveCountsWithEmptyMapDoesNothing) {
    ResourceManager rm;
    rm.awardFromLiveCounts({});
    EXPECT_EQ(rm.getBalance(1), 0u);
}

// trySpend tests
TEST(ResourceManagerTest, TrySpendSucceedsWhenBalanceSufficient) {
    ResourceManager rm;
    rm.awardFromLiveCounts({{1, 100}});
    EXPECT_TRUE(rm.trySpend(1, 60));
    EXPECT_EQ(rm.getBalance(1), 40u);
}

TEST(ResourceManagerTest, TrySpendFailsWhenBalanceInsufficient) {
    ResourceManager rm;
    rm.awardFromLiveCounts({{1, 10}});
    EXPECT_FALSE(rm.trySpend(1, 50));
    EXPECT_EQ(rm.getBalance(1), 10u);  // balance unchanged
}

TEST(ResourceManagerTest, TrySpendFailsForPlayerWithNoBalance) {
    ResourceManager rm;
    EXPECT_FALSE(rm.trySpend(99, 1));
}

TEST(ResourceManagerTest, TrySpendExactBalanceSucceeds) {
    ResourceManager rm;
    rm.awardFromLiveCounts({{1, 50}});
    EXPECT_TRUE(rm.trySpend(1, 50));
    EXPECT_EQ(rm.getBalance(1), 0u);
}

TEST(ResourceManagerTest, TrySpendZeroAlwaysSucceeds) {
    ResourceManager rm;
    rm.addPlayer(1);
    EXPECT_TRUE(rm.trySpend(1, 0));
}

// Thread safety tests

TEST(ResourceManagerTest, ConcurrentAwardsAreThreadSafe) {
    ResourceManager rm;
    const int kThreads = 8;
    const uint64_t kAmountPerThread = 1000;

    std::vector<std::thread> threads;
    for (int i = 0; i < kThreads; ++i) {
        threads.emplace_back([&rm, kAmountPerThread] {
            rm.awardFromLiveCounts({{1, kAmountPerThread}});
        });
    }
    for (auto& t : threads) t.join();

    EXPECT_EQ(rm.getBalance(1), (uint64_t)kThreads * kAmountPerThread);
}

TEST(ResourceManagerTest, ConcurrentSpendDoesNotGoBelowZero) {
    ResourceManager rm;
    rm.awardFromLiveCounts({{1, 100}});

    std::atomic<int> successCount{0};
    std::vector<std::thread> threads;
    for (int i = 0; i < 20; ++i) {
        threads.emplace_back([&rm, &successCount] {
            if (rm.trySpend(1, 10)) ++successCount;
        });
    }
    for (auto& t : threads) t.join();


    EXPECT_EQ(successCount.load(), 10);
    EXPECT_EQ(rm.getBalance(1), 0u);
}
