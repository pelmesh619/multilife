#include "ThreadPool.hpp"

namespace multilife
{

    ThreadPool::ThreadPool(std::size_t threadCount) {
        m_workers.reserve(threadCount);
        for (std::size_t i = 0; i < threadCount; ++i) {
            m_workers.emplace_back([this] { workerLoop(); });
        }
    }

    ThreadPool::~ThreadPool() {
        shutdown();
    }

    std::future<void> ThreadPool::enqueue(std::function<void()> task) {
        std::packaged_task<void()> pt(std::move(task));
        std::future<void> future = pt.get_future();
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_tasks.push(std::move(pt));
        }
        m_cv.notify_one();
        return future;
    }

    void ThreadPool::shutdown() {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_stopping) return;
            m_stopping = true;
        }
        m_cv.notify_all();

        for (auto& worker : m_workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    void ThreadPool::workerLoop() {
        while (true) {
            std::packaged_task<void()> task;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_cv.wait(lock, [this] {
                    return m_stopping.load() || !m_tasks.empty();
                });
                if (m_stopping && m_tasks.empty()) return;
                task = std::move(m_tasks.front());
                m_tasks.pop();
            }
            task();
        }
    }

} // namespace multilife
