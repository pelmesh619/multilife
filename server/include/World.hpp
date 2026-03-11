#pragma once

#include "Types.hpp"
#include "Chunk.hpp"
#include "PlayerCommand.hpp"

#include <unordered_map>
#include <memory>
#include <shared_mutex>
#include <vector>
#include <mutex>

namespace multilife
{

    class World
    {
    public:
        World() = default;

        Chunk& getOrCreateChunk(const ChunkCoord& coord);

        const Chunk* tryGetChunk(const ChunkCoord& coord) const;

        void applyCommands(const std::vector<PlayerCommand>& commands);

        void exchangeBorders();

        std::vector<Chunk*> allChunks();

    private:
        using ChunkMap = std::unordered_map<ChunkCoord, std::unique_ptr<Chunk>, ChunkCoordHash>;

        Chunk& getOrCreateChunkUnlocked(const ChunkCoord& coord);

        mutable std::shared_mutex m_mutex;
        ChunkMap m_chunks;
    };

} // namespace multilife

