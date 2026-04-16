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
        ensureSimulationMarginUnlocked();

        auto getChunkPtr = [this](const ChunkCoord& coord) -> Chunk* {
            auto it = m_chunks.find(coord);
            return it == m_chunks.end() ? nullptr : it->second.get();
        };

        auto readOrDead = [](const Chunk* chunk, std::size_t x, std::size_t y) -> CellState {
            return chunk ? chunk->getCell(x, y) : CellState{};
        };

        for (auto& [coord, chunkPtr] : m_chunks) {
            Chunk& chunk = *chunkPtr;

            const Chunk* north = getChunkPtr({coord.x, coord.y - 1});
            const Chunk* south = getChunkPtr({coord.x, coord.y + 1});
            const Chunk* west = getChunkPtr({coord.x - 1, coord.y});
            const Chunk* east = getChunkPtr({coord.x + 1, coord.y});
            const Chunk* northWest = getChunkPtr({coord.x - 1, coord.y - 1});
            const Chunk* northEast = getChunkPtr({coord.x + 1, coord.y - 1});
            const Chunk* southWest = getChunkPtr({coord.x - 1, coord.y + 1});
            const Chunk* southEast = getChunkPtr({coord.x + 1, coord.y + 1});

            for (std::size_t x = 0; x < ChunkWidth; ++x) {
                chunk.setGhostCell(
                    x + Chunk::GhostBorder,
                    0,
                    readOrDead(north, x, ChunkHeight - 1));
                chunk.setGhostCell(
                    x + Chunk::GhostBorder,
                    ChunkHeight + Chunk::GhostBorder,
                    readOrDead(south, x, 0));
            }

            for (std::size_t y = 0; y < ChunkHeight; ++y) {
                chunk.setGhostCell(
                    0,
                    y + Chunk::GhostBorder,
                    readOrDead(west, ChunkWidth - 1, y));
                chunk.setGhostCell(
                    ChunkWidth + Chunk::GhostBorder,
                    y + Chunk::GhostBorder,
                    readOrDead(east, 0, y));
            }

            chunk.setGhostCell(0, 0, readOrDead(northWest, ChunkWidth - 1, ChunkHeight - 1));
            chunk.setGhostCell(
                ChunkWidth + Chunk::GhostBorder,
                0,
                readOrDead(northEast, 0, ChunkHeight - 1));
            chunk.setGhostCell(
                0,
                ChunkHeight + Chunk::GhostBorder,
                readOrDead(southWest, ChunkWidth - 1, 0));
            chunk.setGhostCell(
                ChunkWidth + Chunk::GhostBorder,
                ChunkHeight + Chunk::GhostBorder,
                readOrDead(southEast, 0, 0));
        }
    }

    std::vector<Chunk*> World::allChunks() {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        std::vector<Chunk*> result;
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

    /* 
        create neighbor chunks if some have a suspicion of giving birth
    */
    void World::ensureSimulationMarginUnlocked() {
        std::vector<ChunkCoord> chunksToCreate;

        for (const auto& [coord, chunkPtr] : m_chunks) {
            bool hasLiveCells = false;
            for (std::size_t y = 0; y < ChunkHeight && !hasLiveCells; ++y) {
                for (std::size_t x = 0; x < ChunkWidth; ++x) {
                    if (chunkPtr->getCell(x, y).alive) {
                        hasLiveCells = true;
                        break;
                    }
                }
            }

            if (!hasLiveCells) {
                continue;
            }

            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (dx == 0 && dy == 0) {
                        continue;
                    }
                    const ChunkCoord neighbor{coord.x + dx, coord.y + dy};
                    if (m_chunks.find(neighbor) == m_chunks.end()) {
                        chunksToCreate.push_back(neighbor);
                    }
                }
            }
        }

        for (const auto& coord : chunksToCreate) {
            getOrCreateChunkUnlocked(coord);
        }
    }

    std::vector<std::pair<ChunkCoord, Chunk*>> World::allChunksWithCoords() {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        std::vector<std::pair<ChunkCoord, Chunk*>> result;
        result.reserve(m_chunks.size());
        for (auto& [coord, chunkPtr] : m_chunks)
            result.emplace_back(coord, chunkPtr.get());
        return result;
    }

    void World::printDebugState() const {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        
        std::cout << "=== World State ===\n";

        for (const auto& id : m_resourceManager.getPlayerIds()) {
            std::cout << "id: " << id << "; bal: " << m_resourceManager.getBalance(id) << '\n';
        }

        std::cout << "Total chunks: " << m_chunks.size() << '\n';
        
        for (const auto& [coord, chunk] : m_chunks) {
            std::cout << "  Chunk [" << coord.x << ", " << coord.y << "]: ";
            
            std::unordered_map<std::uint64_t, int> ownerCounts;
            int totalAlive = 0;
            
            for (std::size_t x = 0; x < ChunkWidth; ++x) {
                for (std::size_t y = 0; y < ChunkHeight; ++y) {
                    CellState cell = chunk->getCell(x, y);
                    if (cell.alive) {
                        totalAlive++;
                        ownerCounts[cell.owner]++;
                    }
                }
            }
            
            std::cout << totalAlive << " alive cells (";
            for (const auto& [owner, count] : ownerCounts) {
                std::cout << "P" << owner << ":" << count << " ";
            }
            std::cout << ")\n";
        }
    }

} // namespace multilife

