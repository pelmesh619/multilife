#pragma once

#include "Chunk.hpp"
#include "NetworkManager.hpp"
#include "Protocol.hpp"
#include "Types.hpp"

#include <cstring>
#include <algorithm>
#include <vector>

namespace multilife {

class WorldSerializer {
public:
    using ChunkList = std::vector<std::pair<ChunkCoord, Chunk*>>;

    // encodes only dirty cells
    static SerializedWorldUpdate serializeDelta(
        std::uint32_t    seqNum,
        const ChunkList& chunks)
    {
        SerializedWorldUpdate out;

        for (auto& [coord, chunk] : chunks) {
            if (!chunk || !chunk->isDirty()) continue;

            const auto& dirty = chunk->dirtyCells();

            std::vector<Entry> entries;
            entries.reserve(dirty.size());

            for (auto& dc : dirty) {
                auto state = chunk->getCell(dc.x, dc.y);
                entries.push_back({dc.x, dc.y,
                    static_cast<std::uint8_t>(state.alive ? 1 : 0),
                    state.owner});
            }

            appendChunk(out, seqNum, proto::kFlagDelta, coord, entries);
            chunk->clearDirty();
        }

        return out;
    }

    // encodes every live cell in every chunk
    static SerializedWorldUpdate serializeFull(
        std::uint32_t    seqNum,
        const ChunkList& chunks)
    {
        SerializedWorldUpdate out;

        for (auto& [coord, chunk] : chunks) {
            if (!chunk) continue;

            std::vector<Entry> entries;
            entries.reserve(128);

            for (std::uint8_t cy = 0; cy < ChunkHeight; ++cy) {
                for (std::uint8_t cx = 0; cx < ChunkWidth; ++cx) {
                    auto state = chunk->getCell(cx, cy);
                    if (state.alive)
                        entries.push_back({cx, cy, 1, state.owner});
                }
            }

            if (entries.empty()) continue;
            appendChunk(out, seqNum, proto::kFlagFull, coord, entries);
        }

        return out;
    }

private:
    template<typename T>
    static void writeLE(std::uint8_t* dst, T value) {
        std::memcpy(dst, &value, sizeof(T));
    }

    struct Entry {
        std::uint8_t  x, y, alive;
        std::uint64_t owner;
    };

    static void appendChunk(
        SerializedWorldUpdate&       out,
        std::uint32_t                seqNum,
        std::uint8_t                 flags,
        const ChunkCoord&            coord,
        std::vector<Entry>&          entries)
    {
        if (entries.empty()) return;

        for (std::size_t offset = 0; offset < entries.size(); offset += proto::kMaxCellsPerPacket) {
            const auto chunkSize = std::min<std::size_t>(
                proto::kMaxCellsPerPacket,
                entries.size() - offset);
            const auto cellCount = static_cast<std::uint16_t>(chunkSize);
            const std::size_t packetBytes =
                proto::kUdpHeader + chunkSize * proto::kUdpCellEntry;

            const std::size_t base = out.data.size();
            out.data.resize(base + packetBytes);
            std::uint8_t* p = out.data.data() + base;

            writeLE(p + proto::kOffSeqNum, seqNum);
            p[proto::kOffFlags] = flags;
            writeLE(p + proto::kOffChunkX, static_cast<std::int32_t>(coord.x));
            writeLE(p + proto::kOffChunkY, static_cast<std::int32_t>(coord.y));
            writeLE(p + proto::kOffCellCount, cellCount);
            p += proto::kUdpHeader;

            for (std::size_t i = 0; i < chunkSize; ++i) {
                const auto& e = entries[offset + i];
                p[0] = e.x;
                p[1] = e.y;
                p[2] = e.alive;
                writeLE(p + 3, e.owner);
                p += proto::kUdpCellEntry;
            }
        }
    }
};

} // namespace multilife
