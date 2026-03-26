#pragma once

#include "Types.hpp"

#include <array>
#include <unordered_map>

namespace multilife
{

    class Chunk {
    public:
        static constexpr std::size_t InnerWidth  = ChunkWidth;
        static constexpr std::size_t InnerHeight = ChunkHeight;
        static constexpr std::size_t GhostBorder = 1;
        static constexpr std::size_t TotalWidth  = InnerWidth + 2 * GhostBorder;
        static constexpr std::size_t TotalHeight = InnerHeight + 2 * GhostBorder;
        static constexpr std::size_t TotalCells  = TotalWidth * TotalHeight;

        Chunk() = default;

        CellState getCell(std::size_t x, std::size_t y) const noexcept;
        void setCell(std::size_t x, std::size_t y, const CellState& state) noexcept;

        CellState getGhostCell(std::size_t storageX, std::size_t storageY) const noexcept;
        void setGhostCell(std::size_t storageX, std::size_t storageY, const CellState& state) noexcept;

        void calculateNext();

        void swapBuffers() noexcept;

        std::unordered_map<PlayerId, std::uint64_t> getLiveCountByPlayer() const;

        void clear();

        struct DirtyCell { std::uint8_t x, y; };

        bool isDirty() const { return !m_dirtyCells.empty(); }
        const std::vector<DirtyCell>& dirtyCells() const { return m_dirtyCells; }
        void clearDirty() { m_dirtyCells.clear(); }

    private:
        using Buffer = std::array<CellState, TotalCells>;

        static std::size_t index(std::size_t storageX, std::size_t storageY) noexcept
        {
            return toIndex(storageX, storageY, TotalWidth);
        }

        std::uint8_t m_currentBufferIndex{0};
        Buffer m_buffers[2]{};
        std::vector<DirtyCell> m_dirtyCells;
    };

} // namespace multilife

