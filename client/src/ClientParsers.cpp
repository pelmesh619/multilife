#include "ClientParsers.h"

#include "ClientProtocol.h"

#include <cstring>

namespace {

template<typename T>
T readLE(const char* data)
{
    T value{};
    std::memcpy(&value, data, sizeof(T));
    return value;
}

} // namespace

namespace multilife::client::parse {

std::optional<ParsedUdpWorldPacket> parseUdpWorldPacket(const QByteArray& datagram)
{
    using namespace multilife::client::proto;

    if (datagram.size() < static_cast<int>(kUdpHeader)) {
        return std::nullopt;
    }

    const auto* bytes = datagram.constData();
    const auto seqNum = readLE<std::uint32_t>(bytes + kOffSeqNum);
    const auto flags = static_cast<std::uint8_t>(*(bytes + kOffFlags));
    const auto chunkX = readLE<std::int32_t>(bytes + kOffChunkX);
    const auto chunkY = readLE<std::int32_t>(bytes + kOffChunkY);
    const auto cellCount = readLE<std::uint16_t>(bytes + kOffCellCount);

    const int expectedSize = static_cast<int>(kUdpHeader + cellCount * kUdpCellEntry);
    if (expectedSize > datagram.size()) {
        return std::nullopt;
    }

    ParsedUdpWorldPacket packet;
    packet.seqNum = seqNum;
    packet.fullSnapshot = flags == kFlagFull;
    packet.chunkX = chunkX;
    packet.chunkY = chunkY;
    packet.cells.reserve(cellCount);

    const auto* cursor = bytes + static_cast<int>(kUdpHeader);
    for (std::uint16_t i = 0; i < cellCount; ++i) {
        const auto localX = static_cast<std::uint8_t>(cursor[0]);
        const auto localY = static_cast<std::uint8_t>(cursor[1]);
        const auto alive = static_cast<std::uint8_t>(cursor[2]) != 0;
        const auto owner = readLE<std::uint64_t>(cursor + 3);
        packet.cells.push_back(UdpCellUpdate{localX, localY, alive, owner});
        cursor += static_cast<int>(kUdpCellEntry);
    }

    return packet;
}

std::optional<ParsedServerStatsMessage> tryParseServerStatsMessage(QByteArray& streamBuffer)
{
    using namespace multilife::client::proto;

    while (true) {
        if (streamBuffer.size() < static_cast<int>(kServerStatsHeader)) {
            return std::nullopt;
        }

        const auto type = static_cast<std::uint8_t>(streamBuffer[0]);
        if (type != kMsgServerStats) {
            streamBuffer.remove(0, 1);
            continue;
        }

        const auto generation = readLE<std::uint32_t>(streamBuffer.constData() + 1);
        const auto playerCount = readLE<std::uint16_t>(streamBuffer.constData() + 5);
        const auto packetSize = static_cast<int>(
            kServerStatsHeader + static_cast<std::size_t>(playerCount) * kServerStatsEntry);
        if (streamBuffer.size() < packetSize) {
            return std::nullopt;
        }

        ParsedServerStatsMessage msg;
        msg.generation = generation;
        msg.players.reserve(playerCount);

        const auto* cursor = streamBuffer.constData() + static_cast<int>(kServerStatsHeader);
        for (std::uint16_t i = 0; i < playerCount; ++i) {
            msg.players.push_back(ServerStatsEntry{
                readLE<std::uint64_t>(cursor),
                readLE<std::uint64_t>(cursor + 8),
                readLE<std::uint64_t>(cursor + 16)
            });
            cursor += static_cast<int>(kServerStatsEntry);
        }

        streamBuffer.remove(0, packetSize);
        return msg;
    }
}

} // namespace multilife::client::parse
