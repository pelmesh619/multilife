#pragma once

#include <QByteArray>
#include <QVector>

#include <cstdint>
#include <optional>

namespace multilife::client::parse {

struct UdpCellUpdate {
    std::uint8_t localX{0};
    std::uint8_t localY{0};
    bool alive{false};
    std::uint64_t owner{0};
};

struct ParsedUdpWorldPacket {
    std::uint32_t seqNum{0};
    bool fullSnapshot{false};
    std::int32_t chunkX{0};
    std::int32_t chunkY{0};
    QVector<UdpCellUpdate> cells;
};

struct ServerStatsEntry {
    std::uint64_t playerId{0};
    std::uint64_t balance{0};
    std::uint64_t liveCells{0};
};

struct ParsedServerStatsMessage {
    std::uint32_t generation{0};
    QVector<ServerStatsEntry> players;
};

std::optional<ParsedUdpWorldPacket> parseUdpWorldPacket(const QByteArray& datagram);
std::optional<ParsedServerStatsMessage> tryParseServerStatsMessage(QByteArray& streamBuffer);

} // namespace multilife::client::parse
