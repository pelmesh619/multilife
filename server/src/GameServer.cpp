#include "GameServer.hpp"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <limits>

namespace multilife
{
    namespace
    {
        template <typename T>
        T readLE(const std::uint8_t* p) {
            T value{};
            std::memcpy(&value, p, sizeof(T));
            return value;
        }

        template <typename T>
        void writeLE(std::uint8_t* p, T value) {
            std::memcpy(p, &value, sizeof(T));
        }

        void rewriteSnapshotSeqNums(std::vector<std::uint8_t>& payload,
                                    std::uint32_t              seqNum) {
            std::size_t offset = 0;
            while (offset + proto::kUdpHeader <= payload.size()) {
                auto* packet = payload.data() + offset;
                const auto cellCount =
                    readLE<std::uint16_t>(packet + proto::kOffCellCount);
                const std::size_t packetSize =
                    proto::kUdpHeader + static_cast<std::size_t>(cellCount) * proto::kUdpCellEntry;
                if (offset + packetSize > payload.size()) {
                    break;
                }
                writeLE(packet + proto::kOffSeqNum, seqNum);
                offset += packetSize;
            }
        }

        std::vector<std::uint8_t> serializeServerStats(
            std::uint32_t generation,
            const ResourceManager& resources,
            const std::unordered_map<PlayerId, std::uint64_t>& liveCounts) {
            auto playerIds = resources.getPlayerIds();
            std::sort(playerIds.begin(), playerIds.end());

            if (playerIds.size() > std::numeric_limits<std::uint16_t>::max()) {
                playerIds.resize(std::numeric_limits<std::uint16_t>::max());
            }

            const auto playerCount = static_cast<std::uint16_t>(playerIds.size());
            const std::size_t size =
                proto::kServerStatsHeader + static_cast<std::size_t>(playerCount) * proto::kServerStatsEntry;

            std::vector<std::uint8_t> payload(size);
            std::uint8_t* p = payload.data();
            p[0] = proto::kMsgServerStats;
            writeLE(p + 1, generation);
            writeLE(p + 5, playerCount);
            p += proto::kServerStatsHeader;

            for (const auto playerId : playerIds) {
                const auto balance = resources.getBalance(playerId);
                const auto it = liveCounts.find(playerId);
                const auto liveCells = (it == liveCounts.end()) ? 0ULL : it->second;
                writeLE(p, playerId);
                writeLE(p + 8, balance);
                writeLE(p + 16, liveCells);
                p += proto::kServerStatsEntry;
            }

            return payload;
        }
    } // namespace

    GameServer::GameServer(std::unique_ptr<NetworkManager> networkManager,
                           std::size_t workerThreads,
                           std::chrono::milliseconds tickInterval)
        : m_networkManager(std::move(networkManager))
        , m_threadPool(workerThreads)
        , m_tickScheduler(tickInterval)
        , m_world(m_resourceManager) {
        std::cout << "Total threads: " << m_threadPool.size() << '\n';
        m_tickScheduler.setTickCallback([this]() { onTick(); });
    }

    GameServer::~GameServer() {
        stop();
    }

    void GameServer::start(std::uint16_t tcpPort, std::uint16_t udpPort) {
        if (m_running.exchange(true)) {
            return;
        }

        if (m_networkManager) {
            m_networkManager->setCommandCallback(
                [this](std::vector<PlayerCommand> commands) { onCommandsFromNetwork(std::move(commands)); });
            m_networkManager->start(tcpPort, udpPort);
        }

        if (auto* bnm = dynamic_cast<BoostNetworkManager*>(m_networkManager.get())) {
            bnm->setFullSnapshotProvider([this](std::uint32_t seqNum) {
                return getFullSnapshotForSeq(seqNum);
            });
        }

        refreshFullSnapshotCache();
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
    }

    void GameServer::onCommandsFromNetwork(std::vector<PlayerCommand> commands) {
        for (auto& cmd : commands) {
            m_commandQueue.push(std::move(cmd));
        }
    }

void GameServer::onTick() {
    ++m_broadcastSeq;
    std::vector<PlayerCommand> batch;
    PlayerCommand cmd;
    while (m_commandQueue.tryPop(cmd)) {
        batch.push_back(std::move(cmd));
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

        // Player actions are applied after simulation step.
        // This way edits are visible immediately and only affect the next tick.
        if (!batch.empty()) {
            world().applyCommands(batch);
        }

        std::unordered_map<PlayerId, std::uint64_t> totalCounts;
        auto allChunksNow = world().allChunks();
        for (Chunk* chunk : allChunksNow) {
            if (!chunk) continue;
            for (const auto& [playerId, count] : chunk->getLiveCountByPlayer()) {
                totalCounts[playerId] += count;
            }
        }
        m_resourceManager.awardFromLiveCounts(totalCounts);
        refreshFullSnapshotCache();

        if (m_broadcastSeq % 1 == 0) {
            world().printDebugState();
        }

        if (m_networkManager) {
            auto chunksWithCoords = world().allChunksWithCoords();
            auto update = WorldSerializer::serializeDelta(m_broadcastSeq, chunksWithCoords);
            if (!update.data.empty())
                m_networkManager->broadcastWorldUpdate(update);

            const auto statsPayload =
                serializeServerStats(m_broadcastSeq, m_resourceManager, totalCounts);
            if (!statsPayload.empty())
                m_networkManager->broadcastServerStats(statsPayload);
        }
    }

    SerializedWorldUpdate GameServer::getFullSnapshotForSeq(std::uint32_t seqNum) const {
        std::lock_guard<std::mutex> lock(m_fullSnapshotMutex);
        auto snapshot = m_cachedFullSnapshot;
        rewriteSnapshotSeqNums(snapshot.data, seqNum);
        return snapshot;
    }

    void GameServer::refreshFullSnapshotCache() {
        auto chunks = world().allChunksWithCoords();
        auto snapshot = WorldSerializer::serializeFull(0, chunks);

        std::lock_guard<std::mutex> lock(m_fullSnapshotMutex);
        m_cachedFullSnapshot = std::move(snapshot);
    }

} // namespace multilife

/*
place 1 1
place 1 2
place 1 3
*/
