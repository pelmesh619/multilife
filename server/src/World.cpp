#include "World.hpp"

namespace multilife
{

    Chunk& World::getOrCreateChunk(const ChunkCoord& coord) {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        return getOrCreateChunkUnlocked(coord);
    }

    const Chunk* World::tryGetChunk(const ChunkCoord& coord) const
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        auto it = m_chunks.find(coord);
        if (it == m_chunks.end()) {
            return nullptr;
        }
        return it->second.get();
    }

    void World::applyCommands(const std::vector<PlayerCommand>& commands) {
        std::unique_lock<std::shared_mutex> lock(m_mutex);

        for (const auto& cmd : commands) {
            const std::int64_t globalX = cmd.x;
            const std::int64_t globalY = cmd.y;

            const int chunkX = static_cast<int>(
                globalX >= 0 ? globalX / static_cast<std::int64_t>(ChunkWidth)
                             : (globalX - static_cast<std::int64_t>(ChunkWidth) + 1) / static_cast<std::int64_t>(ChunkWidth)
            );
            const int chunkY = static_cast<int>(
                globalY >= 0 ? globalY / static_cast<std::int64_t>(ChunkHeight)
                             : (globalY - static_cast<std::int64_t>(ChunkHeight) + 1) / static_cast<std::int64_t>(ChunkHeight)
            );

            const std::size_t localX = static_cast<std::size_t>(
                (globalX % static_cast<std::int64_t>(ChunkWidth) + static_cast<std::int64_t>(ChunkWidth)) % static_cast<std::int64_t>(ChunkWidth)
            );
            const std::size_t localY = static_cast<std::size_t>(
                (globalY % static_cast<std::int64_t>(ChunkHeight) + static_cast<std::int64_t>(ChunkHeight)) % static_cast<std::int64_t>(ChunkHeight)
            );

            ChunkCoord coord{chunkX, chunkY};
            Chunk& chunk = getOrCreateChunkUnlocked(coord);

            CellState cell = chunk.getCell(localX, localY);
            switch (cmd.type) {
            case CommandType::PlaceCell:
                cell.alive = true;
                cell.owner = cmd.playerId;
                break;
            case CommandType::RemoveCell:
                cell.alive = false;
                cell.owner = 0;
                break;
            case CommandType::ToggleCell:
                cell.alive = !cell.alive;
                cell.owner = cell.alive ? cmd.playerId : 0;
                break;
            }

            chunk.setCell(localX, localY, cell);
        }
    }

    void World::exchangeBorders() {
        std::unique_lock<std::shared_mutex> lock(m_mutex);

        // TODO
        (void)lock;
    }

    std::vector<Chunk*> World::allChunks() {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        std::vector<Chunk*>                  result;
        result.reserve(m_chunks.size());
        for (auto& [coord, chunkPtr] : m_chunks) {
            (void)coord;
            result.push_back(chunkPtr.get());
        }
        return result;
    }

    Chunk& World::getOrCreateChunkUnlocked(const ChunkCoord& coord) {
        auto it = m_chunks.find(coord);
        if (it == m_chunks.end()) {
            auto [insertIt, _] = m_chunks.emplace(coord, std::make_unique<Chunk>());
            it                 = insertIt;
        }
        return *it->second;
    }

} // namespace multilife

