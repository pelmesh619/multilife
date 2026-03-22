#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace multilife
{

    class ThreadPool
    {
    public:
        explicit ThreadPool(std::size_t threadCount = std::thread::hardware_concurrency());
        ~ThreadPool();

        std::future<void> enqueue(std::function<void()> task);

        void shutdown();

    private:
        void workerLoop();

        std::vector<std::thread> m_workers;
        std::queue<std::packaged_task<void()>> m_tasks;
        std::mutex m_mutex;
        std::condition_variable m_cv;
        std::atomic<bool> m_stopping{false};
    };

} // namespace multilife
