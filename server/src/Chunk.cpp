#include "Chunk.hpp"

namespace multilife
{

    CellState Chunk::getCell(std::size_t x, std::size_t y) const noexcept {
        const std::size_t storageX = x + GhostBorder;
        const std::size_t storageY = y + GhostBorder;
        const auto& buffer = m_buffers[m_currentBufferIndex];
        return buffer[index(storageX, storageY)];
    }

    void Chunk::setCell(std::size_t x, std::size_t y, const CellState& state) {
        const std::size_t idx = toIndex(
            x + GhostBorder, y + GhostBorder, TotalWidth);
        m_buffers[m_currentBufferIndex][idx] = state;
        m_dirtyCells.push_back({
            static_cast<std::uint8_t>(x),
            static_cast<std::uint8_t>(y)
        });
    }

    CellState Chunk::getGhostCell(std::size_t storageX, std::size_t storageY) const noexcept {
        const auto& buffer = m_buffers[m_currentBufferIndex];
        return buffer[index(storageX, storageY)];
    }

    void Chunk::setGhostCell(std::size_t storageX, std::size_t storageY, const CellState& state) noexcept {
        auto& buffer = m_buffers[m_currentBufferIndex];
        buffer[index(storageX, storageY)] = state;
    }

    void Chunk::calculateNext() {
        const auto& current = m_buffers[m_currentBufferIndex];
        auto& next = m_buffers[1U - m_currentBufferIndex];

        auto neighborAliveCount = [&](std::size_t sx, std::size_t sy) {
            std::uint32_t count = 0;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (dx == 0 && dy == 0)
                        continue;
                    const auto nx = static_cast<std::size_t>(static_cast<int>(sx) + dx);
                    const auto ny = static_cast<std::size_t>(static_cast<int>(sy) + dy);
                    if (current[index(nx, ny)].alive) {
                        ++count;
                    }
                }
            }
            return count;
        };

        for (std::size_t y = 0; y < InnerHeight; ++y) {
            for (std::size_t x = 0; x < InnerWidth; ++x) {
                const std::size_t sx          = x + GhostBorder;
                const std::size_t sy          = y + GhostBorder;
                const CellState& currentCell  = current[index(sx, sy)];
                auto             neighbors    = neighborAliveCount(sx, sy);
                CellState        nextCell     = currentCell;

                if (currentCell.alive) {
                    if (neighbors < 2 || neighbors > 3) {
                        nextCell.alive = false;
                        nextCell.owner = 0;
                        m_dirtyCells.push_back({
                            static_cast<std::uint8_t>(x),
                            static_cast<std::uint8_t>(y)
                        });
                    }
                } else {
                    if (neighbors == 3) {
                        nextCell.alive = true;
                        nextCell.owner = nextOwner;
                        m_dirtyCells.push_back({
                            static_cast<std::uint8_t>(x),
                            static_cast<std::uint8_t>(y)
                        });
                    }
                }

                next[index(sx, sy)] = nextCell;
            }
        }
    }

    void Chunk::swapBuffers() noexcept
    {
        m_currentBufferIndex = 1U - m_currentBufferIndex;
    }

    std::unordered_map<PlayerId, std::uint64_t> Chunk::getLiveCountByPlayer() const
    {
        std::unordered_map<PlayerId, std::uint64_t> counts;
        const auto& buffer = m_buffers[m_currentBufferIndex];

        for (std::size_t y = 0; y < InnerHeight; ++y) {
            for (std::size_t x = 0; x < InnerWidth; ++x) {
                const std::size_t sx = x + GhostBorder;
                const std::size_t sy = y + GhostBorder;
                const CellState& cell = buffer[index(sx, sy)];
                if (cell.alive && cell.owner != 0) {
                    counts[cell.owner] += 1;
                }
            }
        }

        return counts;
    }

    void Chunk::clear() {
        for (auto& buffer : m_buffers) {
            buffer.fill(CellState{});
        }
        m_currentBufferIndex = 0;
        clearDirty();
    }

} // namespace multilife

