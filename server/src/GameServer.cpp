#include "GameServer.hpp"

namespace multilife
{

    GameServer::GameServer(std::unique_ptr<NetworkManager> networkManager,
                           std::size_t workerThreads,
                           std::chrono::milliseconds tickInterval)
        : m_networkManager(std::move(networkManager))
        , m_threadPool(workerThreads)
        , m_tickScheduler(tickInterval) {
        m_tickScheduler.setTickCallback([this]() { onTick(); });
    }

    GameServer::~GameServer() {
        stop();
    }

    void GameServer::start(std::uint16_t port) {
        if (m_running.exchange(true)) {
            return;
        }

        if (m_networkManager) {
            m_networkManager->setCommandCallback(
                [this](std::vector<PlayerCommand> commands) { onCommandsFromNetwork(std::move(commands)); });
            m_networkManager->start(port);
        }

        m_tickScheduler.start();
    }

    void GameServer::stop() {
        if (!m_running.exchange(false)) {
            return;
        }

        m_tickScheduler.stop();

        if (m_networkManager) {
            m_networkManager->stop();
        }

        m_threadPool.shutdown();
    }

    void GameServer::onCommandsFromNetwork(std::vector<PlayerCommand> commands) {
        for (auto& cmd : commands) {
            m_commandQueue.push(std::move(cmd));
        }
    }

    void GameServer::onTick() {
        std::vector<PlayerCommand> batch;
        PlayerCommand cmd;
        while (m_commandQueue.tryPop(cmd)) {
            batch.push_back(std::move(cmd));
        }
        if (!batch.empty()) {
            world().applyCommands(batch);
        }

        world().exchangeBorders();

        auto chunks = world().allChunks();

        {
            std::vector<std::future<void>> futures;
            futures.reserve(chunks.size());
            for (Chunk* chunk : chunks) {
                if (chunk) {
                    futures.push_back(m_threadPool.enqueue([chunk]() {
                        chunk->calculateNext();
                    }));
                }
            }
            // wait for every calculateNext() to finish before any swapBuffers()
            for (auto& f : futures) f.get();
        }

        {
            std::vector<std::future<void>> futures;
            futures.reserve(chunks.size());
            for (Chunk* chunk : chunks) {
                if (chunk) {
                    futures.push_back(m_threadPool.enqueue([chunk]() {
                        chunk->swapBuffers();
                    }));
                }
            }
            for (auto& f : futures) f.get();
        }

        std::unordered_map<PlayerId, std::uint64_t> totalCounts;
        for (Chunk* chunk : chunks) {
            if (!chunk) continue;
            for (const auto& [playerId, count] : chunk->getLiveCountByPlayer()) {
                totalCounts[playerId] += count;
            }
        }
        m_resourceManager.awardFromLiveCounts(totalCounts);
    }

} // namespace multilife