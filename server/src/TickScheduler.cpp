#include "TickScheduler.hpp"

namespace multilife
{

    TickScheduler::TickScheduler(Duration tickInterval)
        : m_tickInterval(tickInterval) {
    }

    TickScheduler::~TickScheduler() {
        stop();
    }

    void TickScheduler::setTickCallback(TickCallback callback) {
        m_tickCallback = std::move(callback);
    }

    void TickScheduler::start() {
        if (m_running.exchange(true)) {
            return; // already running
        }

        m_thread = std::thread([this]() { runLoop(); });
    }

    void TickScheduler::stop() {
        if (!m_running.exchange(false)) {
            return;
        }

        if (m_thread.joinable()) {
            m_thread.join();
        }
    }

    void TickScheduler::runLoop() {
        auto nextTick = Clock::now();
        while (m_running.load(std::memory_order_relaxed)) {
            nextTick += m_tickInterval;

            if (m_tickCallback) {
                m_tickCallback();
            }

            std::this_thread::sleep_until(nextTick);
        }
    }

} // namespace multilife

