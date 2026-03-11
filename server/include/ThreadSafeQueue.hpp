#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>

namespace multilife
{

    template <typename T>
    class ThreadSafeQueue {
    public:
        ThreadSafeQueue() = default;

        ThreadSafeQueue(const ThreadSafeQueue&) = delete;
        ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;

        void push(T value) {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_queue.push(std::move(value));
            }
            m_cv.notify_one();
        }

        template <typename StopPredicate>
        bool waitAndPop(T& out, StopPredicate&& shouldStop) {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [&]() { return !m_queue.empty() || shouldStop(); });

            if (m_queue.empty())
            {
                return false;
            }

            out = std::move(m_queue.front());
            m_queue.pop();
            return true;
        }

        bool tryPop(T& out) {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_queue.empty()) {
                return false;
            }
            out = std::move(m_queue.front());
            m_queue.pop();
            return true;
        }

        [[nodiscard]] bool empty() const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_queue.empty();
        }

    private:
        mutable std::mutex          m_mutex;
        std::condition_variable     m_cv;
        std::queue<T>               m_queue;
    };

} // namespace multilife

