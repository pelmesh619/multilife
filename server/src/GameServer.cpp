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
            m_threadPool.enqueue([this, localBatch = std::move(batch)]() mutable {
                world().applyCommands(localBatch);
            });
        }

        world().exchangeBorders();

        auto chunks = world().allChunks();
        for (Chunk* chunk : chunks) {
            if (!chunk) {
                continue;
            }
            m_threadPool.enqueue([chunk]() {
                chunk->calculateNext();
            });
        }

        for (Chunk* chunk : chunks) {
            if (!chunk) {
                continue;
            }
            m_threadPool.enqueue([chunk]() {
                chunk->swapBuffers();
            });
        }

        std::unordered_map<PlayerId, std::uint64_t> totalCounts;
        for (Chunk* chunk : chunks) {
            if (!chunk) {
                continue;
            }
            auto counts = chunk->getLiveCountByPlayer();
            for (const auto& [playerId, count] : counts) {
                totalCounts[playerId] += count;
            }
        }
        m_resourceManager.awardFromLiveCounts(totalCounts);
    }

} // namespace multilife