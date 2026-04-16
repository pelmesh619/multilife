#pragma once

#include "Types.hpp"
#include "PlayerCommand.hpp"
#include "ThreadSafeQueue.hpp"
#include "ThreadPool.hpp"
#include "World.hpp"
#include "WorldSerializer.hpp"
#include "ResourceManager.hpp"
#include "TickScheduler.hpp"
#include "NetworkManager.hpp"
#include "BoostNetworkManager.hpp"

#include <memory>
#include <atomic>
#include <mutex>
#include <vector>

namespace multilife
{

    class GameServer {
    public:
        GameServer(std::unique_ptr<NetworkManager> networkManager,
                   std::size_t workerThreads,
                   std::chrono::milliseconds tickInterval);

        ~GameServer();

        GameServer(const GameServer&) = delete;
        GameServer& operator=(const GameServer&) = delete;

        void start(std::uint16_t tcpPort, std::uint16_t udpPort);

        void stop();

        [[nodiscard]] bool isRunning() const noexcept {
            return m_running.load(std::memory_order_relaxed);
        }

        World& world() noexcept {
            return m_world;
        }
        ResourceManager& resources() noexcept {
            return m_resourceManager;
        }

        NetworkManager& networkManager() {
            if (!m_networkManager) {
                throw std::runtime_error("Network manager not initialized");
            }
            return *m_networkManager;
        }

    private:
        void onCommandsFromNetwork(std::vector<PlayerCommand> commands);
        void onTick();
        SerializedWorldUpdate getFullSnapshotForSeq(std::uint32_t seqNum) const;
        void refreshFullSnapshotCache();

        std::unique_ptr<NetworkManager> m_networkManager;
        ThreadPool                      m_threadPool;
        World                           m_world;
        ResourceManager                 m_resourceManager;
        TickScheduler                   m_tickScheduler;

        ThreadSafeQueue<PlayerCommand>  m_commandQueue;
        std::atomic<bool>               m_running{false};

        mutable std::mutex              m_fullSnapshotMutex;
        SerializedWorldUpdate           m_cachedFullSnapshot;
        std::uint32_t m_broadcastSeq{0};
    };

} // namespace multilife

