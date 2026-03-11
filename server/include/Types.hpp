#pragma once

#include <cstdint>
#include <array>
#include <functional>

namespace multilife
{
    using PlayerId = std::uint64_t;

    struct CellState {
        bool     alive{false};
        PlayerId owner{0}; // 0 means "no owner"
    };

    struct ChunkCoord {
        int x{0};
        int y{0};

        bool operator==(const ChunkCoord& other) const noexcept
        {
            return x == other.x && y == other.y;
        }
    };

    struct ChunkCoordHash {
        std::size_t operator()(const ChunkCoord& coord) const noexcept
        {
            std::size_t hx = std::hash<int>{}(coord.x);
            std::size_t hy = std::hash<int>{}(coord.y);
            return hx ^ (hy << 1);
        }
    };

    inline constexpr std::size_t ChunkWidth  = 64;
    inline constexpr std::size_t ChunkHeight = 64;

    inline constexpr std::size_t toIndex(std::size_t x, std::size_t y, std::size_t width) noexcept
    {
        return y * width + x;
    }

} // namespace multilife

