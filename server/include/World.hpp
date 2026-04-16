#pragma once

#include "Types.hpp"
#include "Chunk.hpp"
#include "ResourceManager.hpp"
#include "PlayerCommand.hpp"

#include <unordered_map>
#include <memory>
#include <shared_mutex>
#include <vector>

namespace multilife
{
    constexpr uint64_t kPlaceCost = 2;
    constexpr uint64_t kRemoveAward = 1;
    constexpr uint64_t kAliveCellAward = 1;
    constexpr uint64_t kStartBalance = 10;

    class World
    {
    public:
        World() = default;
        World(ResourceManager& resourceManager) : m_resourceManager(resourceManager) {};

        Chunk& getOrCreateChunk(const ChunkCoord& coord);

        const Chunk* tryGetChunk(const ChunkCoord& coord) const;

        void applyCommands(const std::vector<PlayerCommand>& commands);

        void exchangeBorders();

        std::vector<Chunk*> allChunks();
        std::vector<std::pair<ChunkCoord, Chunk*>> allChunksWithCoords();

        void printDebugState() const;

    private:
        using ChunkMap = std::unordered_map<ChunkCoord, std::unique_ptr<Chunk>, ChunkCoordHash>;

        Chunk& getOrCreateChunkUnlocked(const ChunkCoord& coord);
        void ensureSimulationMarginUnlocked();

        mutable std::shared_mutex m_mutex;
        ChunkMap m_chunks;
        ResourceManager& m_resourceManager;
    };

} // namespace multilife

