#include <gtest/gtest.h>
#include "ThreadSafeQueue.hpp"
#include <thread>
#include <atomic>
#include <vector>

using namespace multilife;

// Basic operations tests

TEST(ThreadSafeQueueTest, NewQueueIsEmpty) {
    ThreadSafeQueue<int> q;
    EXPECT_TRUE(q.empty());
}

TEST(ThreadSafeQueueTest, PushMakesQueueNonEmpty) {
    ThreadSafeQueue<int> q;
    q.push(42);
    EXPECT_FALSE(q.empty());
}

TEST(ThreadSafeQueueTest, TryPopSucceedsWhenNonEmpty) {
    ThreadSafeQueue<int> q;
    q.push(7);
    int val = 0;
    EXPECT_TRUE(q.tryPop(val));
    EXPECT_EQ(val, 7);
}

TEST(ThreadSafeQueueTest, TryPopFailsWhenEmpty) {
    ThreadSafeQueue<int> q;
    int val = 0;
    EXPECT_FALSE(q.tryPop(val));
}

TEST(ThreadSafeQueueTest, TryPopEmptiesQueue) {
    ThreadSafeQueue<int> q;
    q.push(1);
    int val;
    q.tryPop(val);
    EXPECT_TRUE(q.empty());
}

TEST(ThreadSafeQueueTest, FIFOOrdering) {
    ThreadSafeQueue<int> q;
    q.push(1);
    q.push(2);
    q.push(3);
    int a, b, c;
    q.tryPop(a);
    q.tryPop(b);
    q.tryPop(c);
    EXPECT_EQ(a, 1);
    EXPECT_EQ(b, 2);
    EXPECT_EQ(c, 3);
}

// waitAndPop test

TEST(ThreadSafeQueueTest, WaitAndPopBlocksUntilItemAvailable) {
    ThreadSafeQueue<int> q;
    int result = 0;
    std::atomic<bool> done{false};

    std::thread consumer([&] {
        int val;
        q.waitAndPop(val, [&done] { return done.load(); });
        result = val;
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    q.push(99);
    consumer.join();
    EXPECT_EQ(result, 99);
}

TEST(ThreadSafeQueueTest, WaitAndPopReturnsWhenStopPredicateTriggered) {
    ThreadSafeQueue<int> q;
    std::atomic<bool> stop{false};
    bool returned = false;

    std::thread consumer([&] {
        int val;
        // Will block
        q.waitAndPop(val, [&stop] { return stop.load(); });
        returned = true;
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    stop = true;
    // Push to wake the condition variable
    q.push(0);
    consumer.join();
    EXPECT_TRUE(returned);
}

// Thread safety test

TEST(ThreadSafeQueueTest, ConcurrentPushesAreThreadSafe) {
    ThreadSafeQueue<int> q;
    const int kItems = 1000;
    const int kThreads = 8;

    std::vector<std::thread> producers;
    for (int t = 0; t < kThreads; ++t) {
        producers.emplace_back([&q, kItems] {
            for (int i = 0; i < kItems; ++i) q.push(i);
        });
    }
    for (auto& t : producers) t.join();

    int count = 0;
    int val;
    while (q.tryPop(val)) ++count;
    EXPECT_EQ(count, kThreads * kItems);
}
