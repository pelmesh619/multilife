#include "ClientParsers.h"
#include "ClientProtocol.h"

#include <gtest/gtest.h>

#include <cstring>

namespace {

template<typename T>
void appendLE(QByteArray& data, T value)
{
    const auto oldSize = data.size();
    data.resize(oldSize + static_cast<int>(sizeof(T)));
    std::memcpy(data.data() + oldSize, &value, sizeof(T));
}

QByteArray makeUdpDatagram(std::uint32_t seq,
                           std::uint8_t flags,
                           std::int32_t chunkX,
                           std::int32_t chunkY,
                           const QVector<multilife::client::parse::UdpCellUpdate>& cells)
{
    QByteArray out;
    out.reserve(static_cast<int>(
        multilife::client::proto::kUdpHeader +
        cells.size() * static_cast<int>(multilife::client::proto::kUdpCellEntry)));

    appendLE(out, seq);
    out.append(static_cast<char>(flags));
    appendLE(out, chunkX);
    appendLE(out, chunkY);
    appendLE(out, static_cast<std::uint16_t>(cells.size()));

    for (const auto& cell : cells) {
        out.append(static_cast<char>(cell.localX));
        out.append(static_cast<char>(cell.localY));
        out.append(static_cast<char>(cell.alive ? 1 : 0));
        appendLE(out, cell.owner);
    }
    return out;
}

QByteArray makeServerStatsMessage(
    std::uint32_t generation,
    const QVector<multilife::client::parse::ServerStatsEntry>& players)
{
    QByteArray out;
    out.append(static_cast<char>(multilife::client::proto::kMsgServerStats));
    appendLE(out, generation);
    appendLE(out, static_cast<std::uint16_t>(players.size()));
    for (const auto& p : players) {
        appendLE(out, p.playerId);
        appendLE(out, p.balance);
        appendLE(out, p.liveCells);
    }
    return out;
}

} // namespace

TEST(ClientParsersTest, ParseUdpWorldPacketValid)
{
    using namespace multilife::client::parse;
    using namespace multilife::client::proto;

    const QByteArray datagram = makeUdpDatagram(
        42,
        kFlagDelta,
        -2,
        3,
        {{1, 2, true, 100}, {4, 5, false, 0}});

    const auto parsed = parseUdpWorldPacket(datagram);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->seqNum, static_cast<std::uint32_t>(42));
    EXPECT_FALSE(parsed->fullSnapshot);
    EXPECT_EQ(parsed->chunkX, -2);
    EXPECT_EQ(parsed->chunkY, 3);
    ASSERT_EQ(parsed->cells.size(), 2);
    EXPECT_EQ(parsed->cells[0].localX, static_cast<std::uint8_t>(1));
    EXPECT_EQ(parsed->cells[0].localY, static_cast<std::uint8_t>(2));
    EXPECT_TRUE(parsed->cells[0].alive);
    EXPECT_EQ(parsed->cells[0].owner, static_cast<std::uint64_t>(100));
    EXPECT_FALSE(parsed->cells[1].alive);
}

TEST(ClientParsersTest, ParseUdpWorldPacketRejectsTruncatedPayload)
{
    using namespace multilife::client::parse;
    using namespace multilife::client::proto;

    auto datagram = makeUdpDatagram(1, kFlagFull, 0, 0, {{7, 8, true, 55}});
    datagram.chop(3);

    const auto parsed = parseUdpWorldPacket(datagram);
    EXPECT_FALSE(parsed.has_value());
}

TEST(ClientParsersTest, ParseServerStatsMessageValidWithPrefixNoise)
{
    using namespace multilife::client::parse;

    QByteArray stream;
    stream.append('\x01');
    stream.append('\x02');
    stream.append(makeServerStatsMessage(
        77,
        {
            {11, 123, 4},
            {22, 456, 8},
        }));

    const auto parsed = tryParseServerStatsMessage(stream);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->generation, static_cast<std::uint32_t>(77));
    ASSERT_EQ(parsed->players.size(), 2);
    EXPECT_EQ(parsed->players[0].playerId, static_cast<std::uint64_t>(11));
    EXPECT_EQ(parsed->players[0].balance, static_cast<std::uint64_t>(123));
    EXPECT_EQ(parsed->players[0].liveCells, static_cast<std::uint64_t>(4));
    EXPECT_EQ(parsed->players[1].playerId, static_cast<std::uint64_t>(22));
    EXPECT_EQ(stream.size(), 0);
}

TEST(ClientParsersTest, ParseServerStatsMessageIncompleteIsBuffered)
{
    using namespace multilife::client::parse;

    const QByteArray full = makeServerStatsMessage(12, {{1, 10, 3}});
    QByteArray partial = full.left(full.size() - 5);

    const auto missing = tryParseServerStatsMessage(partial);
    EXPECT_FALSE(missing.has_value());
    EXPECT_EQ(partial.size(), full.size() - 5);

    partial.append(full.right(5));
    const auto parsed = tryParseServerStatsMessage(partial);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->generation, static_cast<std::uint32_t>(12));
    ASSERT_EQ(parsed->players.size(), 1);
    EXPECT_EQ(parsed->players[0].playerId, static_cast<std::uint64_t>(1));
    EXPECT_EQ(parsed->players[0].balance, static_cast<std::uint64_t>(10));
    EXPECT_EQ(parsed->players[0].liveCells, static_cast<std::uint64_t>(3));
    EXPECT_EQ(partial.size(), 0);
}
