#pragma once

#include "ThreadPool.hpp"
#include "ThreadSafeQueue.hpp"
#include "PlayerCommand.hpp"

#include <atomic>
#include <chrono>
#include <thread>
#include <functional>
#include <vector>

namespace multilife
{

    class World;
    class ResourceManager;

    class TickScheduler
    {
    public:
        using Clock = std::chrono::steady_clock;
        using Duration = std::chrono::milliseconds;

        using TickCallback = std::function<void()>;

        explicit TickScheduler(Duration tickInterval);
        ~TickScheduler();

        TickScheduler(const TickScheduler&) = delete;
        TickScheduler& operator=(const TickScheduler&) = delete;

        void setTickCallback(TickCallback callback);

        void start();

        void stop();

        [[nodiscard]] bool isRunning() const noexcept {
            return m_running.load(std::memory_order_relaxed);
        }

    private:
        void runLoop();

        Duration m_tickInterval;
        TickCallback m_tickCallback;
        std::thread m_thread;
        std::atomic<bool> m_running{false};
    };

} // namespace multilife

