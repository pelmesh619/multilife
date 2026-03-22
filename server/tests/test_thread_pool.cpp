#include <gtest/gtest.h>
#include "ThreadPool.hpp"
#include <atomic>
#include <vector>
#include <mutex>

using namespace multilife;

TEST(ThreadPoolTest, EnqueuedTasksExecute) {
    ThreadPool pool(2);
    std::atomic<int> counter{0};
    const int kTasks = 100;

    for (int i = 0; i < kTasks; ++i) {
        pool.enqueue([&counter] { ++counter; });
    }

    pool.shutdown();
    EXPECT_EQ(counter.load(), kTasks);
}

TEST(ThreadPoolTest, TasksRunConcurrently) {
    ThreadPool pool(4);
    std::atomic<int> concurrent{0};
    std::atomic<int> maxConcurrent{0};
    std::mutex mu;

    for (int i = 0; i < 4; ++i) {
        pool.enqueue([&] {
            int val = ++concurrent;
            {
                std::lock_guard<std::mutex> lock(mu);
                if (val > maxConcurrent) maxConcurrent = val;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            --concurrent;
        });
    }

    pool.shutdown();
    EXPECT_GT(maxConcurrent.load(), 1);
}

TEST(ThreadPoolTest, ShutdownIsIdempotent) {
    ThreadPool pool(2);
    pool.enqueue([] {});
    pool.shutdown();
    EXPECT_NO_THROW(pool.shutdown());
}

TEST(ThreadPoolTest, DestructorCallsShutdown) {
    std::atomic<int> counter{0};
    {
        ThreadPool pool(2);
        for (int i = 0; i < 10; ++i)
            pool.enqueue([&counter] { ++counter; });
    }
    EXPECT_EQ(counter.load(), 10);
}

TEST(ThreadPoolTest, SingleThreadPoolExecutesAllTasks) {
    ThreadPool pool(1);
    std::vector<int> results;
    std::mutex mu;

    for (int i = 0; i < 5; ++i) {
        pool.enqueue([&, i] {
            std::lock_guard<std::mutex> lock(mu);
            results.push_back(i);
        });
    }

    pool.shutdown();
    EXPECT_EQ((int)results.size(), 5);
}
