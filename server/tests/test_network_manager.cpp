#include <gtest/gtest.h>
#include "BoostNetworkManager.hpp"
#include "WorldSerializer.hpp"
#include "Protocol.hpp"
#include "Types.hpp"
#include "PlayerCommand.hpp"
#include "Chunk.hpp"

#include <boost/asio.hpp>
#include <atomic>
#include <chrono>
#include <cstring>
#include <thread>
#include <vector>

using namespace multilife;
using boost::asio::ip::tcp;
using boost::asio::ip::udp;
namespace asio = boost::asio;


template<typename T>
static void writeLE(std::uint8_t* p, T v) { std::memcpy(p, &v, sizeof(T)); }

template<typename T>
static T readLE(const std::uint8_t* p) { T v{}; std::memcpy(&v, p, sizeof(T)); return v; }

static std::array<std::uint8_t, proto::kHandshakeSize>
makeHandshake(PlayerId id)
{
    std::array<std::uint8_t, proto::kHandshakeSize> buf{};
    writeLE(buf.data(),     proto::kMagic);
    writeLE(buf.data() + 4, id);
    return buf;
}


static std::array<std::uint8_t, proto::kCommandSize>
makeCommand(std::uint8_t type, PlayerId id, std::int64_t x, std::int64_t y)
{
    std::array<std::uint8_t, proto::kCommandSize> buf{};
    buf[0] = type;
    writeLE(buf.data() + 1,  id);
    writeLE(buf.data() + 9,  x);
    writeLE(buf.data() + 17, y);
    return buf;
}

// Synchronous mock-client

static constexpr std::uint16_t kTestPort = 19877;

struct TestClient {
    asio::io_context ioc;
    tcp::socket      tcpSock{ioc};
    udp::socket      udpSock{ioc};

    void connectTcp(std::uint16_t port = kTestPort) {
        tcpSock.connect(boost::asio::ip::tcp::endpoint(
            boost::asio::ip::make_address("127.0.0.1"), port));
    }

    void openUdp() {
        udpSock.open(udp::v4());
        udpSock.non_blocking(true);
    }

    void sendHandshake(PlayerId id) {
        auto buf = makeHandshake(id);
        asio::write(tcpSock, asio::buffer(buf));
    }

    void sendCommand(std::uint8_t type, PlayerId id,
                     std::int64_t x, std::int64_t y) {
        auto buf = makeCommand(type, id, x, y);
        asio::write(tcpSock, asio::buffer(buf));
    }

    void sendResyncRequest() {
        std::array<std::uint8_t, 1> buf{proto::kMsgResyncReq};
        asio::write(tcpSock, asio::buffer(buf));
    }

    // Attempt to receive one UDP datagram
    std::vector<std::uint8_t> recvUdp(int timeoutMs = 200) {
        std::vector<std::uint8_t> buf(proto::kMaxUdpPayload);
        auto deadline = std::chrono::steady_clock::now()
                      + std::chrono::milliseconds(timeoutMs);
        while (std::chrono::steady_clock::now() < deadline) {
            boost::system::error_code ec;
            udp::endpoint sender;
            std::size_t n = udpSock.receive_from(asio::buffer(buf), sender, 0, ec);
            if (!ec && n > 0) {
                buf.resize(n);
                return buf;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return {};
    }

    void close() {
        boost::system::error_code ec;
        tcpSock.close(ec);
        if (udpSock.is_open()) udpSock.close(ec);
    }
};

// Fixture

class NetworkManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        nm = std::make_unique<BoostNetworkManager>();
        nm->setCommandCallback([this](std::vector<PlayerCommand> cmds) {
            for (auto& c : cmds) received.push_back(c);
        });
        nm->start(kTestPort);
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }

    void TearDown() override { nm->stop(); }

    template<typename Pred>
    bool waitFor(Pred pred, int ms = 500) {
        auto dl = std::chrono::steady_clock::now()
                + std::chrono::milliseconds(ms);
        while (!pred()) {
            if (std::chrono::steady_clock::now() >= dl) return false;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return true;
    }

    std::unique_ptr<BoostNetworkManager> nm;
    std::vector<PlayerCommand>           received;
};

// Handshake tests

TEST_F(NetworkManagerTest, ValidHandshakeAccepted) {
    TestClient c;
    c.connectTcp();
    c.sendHandshake(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_NO_THROW(c.sendCommand(proto::kCmdPlace, 1, 0, 0));
    c.close();
}

TEST_F(NetworkManagerTest, InvalidMagicDropsConnection) {
    TestClient c;
    c.connectTcp();
    std::array<std::uint8_t, proto::kHandshakeSize> buf{};
    writeLE(buf.data(),     static_cast<std::uint32_t>(0xDEADBEEF));
    writeLE(buf.data() + 4, static_cast<std::uint64_t>(1));
    asio::write(c.tcpSock, asio::buffer(buf));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::array<std::uint8_t, 1> dummy{};
    boost::system::error_code ec;
    c.tcpSock.read_some(asio::buffer(dummy), ec);
    EXPECT_TRUE(ec == asio::error::eof || ec == asio::error::connection_reset);
    c.close();
}

// Command delivery tests

TEST_F(NetworkManagerTest, PlaceCellDelivered) {
    TestClient c; c.connectTcp(); c.sendHandshake(2);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    c.sendCommand(proto::kCmdPlace, 2, 10, 20);
    ASSERT_TRUE(waitFor([this]{ return !received.empty(); }));
    EXPECT_EQ(received[0].type,     CommandType::PlaceCell);
    EXPECT_EQ(received[0].playerId, 2u);
    EXPECT_EQ(received[0].x,        10);
    EXPECT_EQ(received[0].y,        20);
    c.close();
}

TEST_F(NetworkManagerTest, RemoveCellDelivered) {
    TestClient c; c.connectTcp(); c.sendHandshake(3);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    c.sendCommand(proto::kCmdRemove, 3, -1, -2);
    ASSERT_TRUE(waitFor([this]{ return !received.empty(); }));
    EXPECT_EQ(received[0].type, CommandType::RemoveCell);
    c.close();
}

TEST_F(NetworkManagerTest, ToggleCellDelivered) {
    TestClient c; c.connectTcp(); c.sendHandshake(4);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    c.sendCommand(proto::kCmdToggle, 4, 5, 5);
    ASSERT_TRUE(waitFor([this]{ return !received.empty(); }));
    EXPECT_EQ(received[0].type, CommandType::ToggleCell);
    c.close();
}

TEST_F(NetworkManagerTest, SpoofedPlayerIdIgnored) {
    TestClient c; c.connectTcp(); c.sendHandshake(5);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    c.sendCommand(proto::kCmdPlace, 99, 0, 0);  // claims to be player 99
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    EXPECT_TRUE(received.empty());
    c.close();
}

TEST_F(NetworkManagerTest, MultipleCommandsDeliveredInOrder) {
    TestClient c; c.connectTcp(); c.sendHandshake(6);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    c.sendCommand(proto::kCmdPlace, 6, 1, 0);
    c.sendCommand(proto::kCmdPlace, 6, 2, 0);
    c.sendCommand(proto::kCmdPlace, 6, 3, 0);
    ASSERT_TRUE(waitFor([this]{ return received.size() >= 3; }));
    EXPECT_EQ(received[0].x, 1);
    EXPECT_EQ(received[1].x, 2);
    EXPECT_EQ(received[2].x, 3);
    c.close();
}

TEST_F(NetworkManagerTest, MultipleClientsDeliverIndependently) {
    TestClient c1, c2;
    c1.connectTcp(); c1.sendHandshake(10);
    c2.connectTcp(); c2.sendHandshake(11);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    c1.sendCommand(proto::kCmdPlace, 10, 1, 0);
    c2.sendCommand(proto::kCmdPlace, 11, 2, 0);
    ASSERT_TRUE(waitFor([this]{ return received.size() >= 2; }));
    std::vector<PlayerId> ids;
    for (auto& cmd : received) ids.push_back(cmd.playerId);
    EXPECT_NE(std::find(ids.begin(), ids.end(), 10u), ids.end());
    EXPECT_NE(std::find(ids.begin(), ids.end(), 11u), ids.end());
    c1.close(); c2.close();
}

// Resync request test

TEST_F(NetworkManagerTest, ResyncRequestDoesNotCrashServer) {
    TestClient c; c.connectTcp(); c.sendHandshake(20);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    EXPECT_NO_THROW(c.sendResyncRequest());
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_TRUE(nm->isRunning());
    c.close();
}

TEST_F(NetworkManagerTest, ResyncRequestFollowedByCommandStillDelivered) {
    TestClient c; c.connectTcp(); c.sendHandshake(21);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    c.sendResyncRequest();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    c.sendCommand(proto::kCmdPlace, 21, 7, 7);
    ASSERT_TRUE(waitFor([this]{ return !received.empty(); }));
    EXPECT_EQ(received[0].x, 7);
    c.close();
}

// WorldSerializer, delta test

TEST(WorldSerializerTest, DeltaEmptyWhenNoDirtyCells) {
    Chunk chunk;
    WorldSerializer::ChunkList list{{{0, 0}, &chunk}};
    auto result = WorldSerializer::serializeDelta(1, list);
    EXPECT_TRUE(result.data.empty());
}

TEST(WorldSerializerTest, DeltaContainsDirtyCell) {
    Chunk chunk;
    chunk.setCell(5, 5, {true, 42});
    ASSERT_TRUE(chunk.isDirty());

    WorldSerializer::ChunkList list{{{0, 0}, &chunk}};
    auto result = WorldSerializer::serializeDelta(1, list);
    EXPECT_FALSE(result.data.empty());

    
    const std::uint8_t* p = result.data.data();
    std::uint32_t seq   = readLE<std::uint32_t>(p + proto::kOffSeqNum);
    std::uint8_t  flags = p[proto::kOffFlags];
    std::uint16_t count = readLE<std::uint16_t>(p + proto::kOffCellCount);

    EXPECT_EQ(seq,   1u);
    EXPECT_EQ(flags, proto::kFlagDelta);
    EXPECT_EQ(count, 1u);

    
    const std::uint8_t* cell = p + proto::kUdpHeader;
    EXPECT_EQ(cell[0], 5);
    EXPECT_EQ(cell[1], 5);
    EXPECT_EQ(cell[2], 1);
    EXPECT_EQ(readLE<std::uint64_t>(cell + 3), 42u);
}

TEST(WorldSerializerTest, DeltaClearsDirtyAfterSerialise) {
    Chunk chunk;
    chunk.setCell(1, 1, {true, 1});

    WorldSerializer::ChunkList list{{{0, 0}, &chunk}};
    WorldSerializer::serializeDelta(1, list);

    EXPECT_FALSE(chunk.isDirty());

    auto result2 = WorldSerializer::serializeDelta(2, list);
    EXPECT_TRUE(result2.data.empty());
}

TEST(WorldSerializerTest, DeltaEncodesDeadCell) {
    Chunk chunk;
    chunk.setCell(3, 3, {true, 1});
    chunk.clearDirty(); 
    chunk.setCell(3, 3, {false, 0}); 

    WorldSerializer::ChunkList list{{{0, 0}, &chunk}};
    auto result = WorldSerializer::serializeDelta(1, list);
    ASSERT_FALSE(result.data.empty());

    const std::uint8_t* cell = result.data.data() + proto::kUdpHeader;
    EXPECT_EQ(cell[2], 0);
    EXPECT_EQ(readLE<std::uint64_t>(cell + 3), 0u); 
}

TEST(WorldSerializerTest, DeltaSequenceNumberEmbedded) {
    Chunk chunk;
    chunk.setCell(0, 0, {true, 1});

    WorldSerializer::ChunkList list{{{0, 0}, &chunk}};
    auto result = WorldSerializer::serializeDelta(42, list);
    ASSERT_FALSE(result.data.empty());

    std::uint32_t seq =
        readLE<std::uint32_t>(result.data.data() + proto::kOffSeqNum);
    EXPECT_EQ(seq, 42u);
}

// WorldSerializer, full snapshot tests

TEST(WorldSerializerTest, FullSnapshotEmptyWhenNoLiveCells) {
    Chunk chunk;
    WorldSerializer::ChunkList list{{{0, 0}, &chunk}};
    auto result = WorldSerializer::serializeFull(1, list);
    EXPECT_TRUE(result.data.empty());
}

TEST(WorldSerializerTest, FullSnapshotContainsAllLiveCells) {
    Chunk chunk;
    chunk.setCell(1, 1, {true, 10});
    chunk.setCell(2, 2, {true, 10});
    chunk.setCell(3, 3, {true, 10});
    chunk.clearDirty();

    WorldSerializer::ChunkList list{{{0, 0}, &chunk}};
    auto result = WorldSerializer::serializeFull(7, list);
    ASSERT_FALSE(result.data.empty());

    const std::uint8_t* p = result.data.data();
    EXPECT_EQ(p[proto::kOffFlags], proto::kFlagFull);
    std::uint16_t count =
        readLE<std::uint16_t>(p + proto::kOffCellCount);
    EXPECT_EQ(count, 3u);
}

TEST(WorldSerializerTest, FullSnapshotDoesNotClearDirty) {
    Chunk chunk;
    chunk.setCell(0, 0, {true, 1});
    ASSERT_TRUE(chunk.isDirty());

    WorldSerializer::ChunkList list{{{0, 0}, &chunk}};
    WorldSerializer::serializeFull(1, list);

    EXPECT_TRUE(chunk.isDirty());
}

TEST(WorldSerializerTest, FullAndDeltaCarryCorrectFlags) {
    Chunk chunk;
    chunk.setCell(5, 5, {true, 1});

    WorldSerializer::ChunkList list{{{0, 0}, &chunk}};

    auto full  = WorldSerializer::serializeFull(1, list);
    auto delta = WorldSerializer::serializeDelta(2, list);

    ASSERT_FALSE(full.data.empty());
    ASSERT_FALSE(delta.data.empty());

    EXPECT_EQ(full.data[proto::kOffFlags],  proto::kFlagFull);
    EXPECT_EQ(delta.data[proto::kOffFlags], proto::kFlagDelta);
}

// Lifecycle tests

TEST_F(NetworkManagerTest, StopIsIdempotent) {
    nm->stop();
    EXPECT_NO_THROW(nm->stop());
}

TEST_F(NetworkManagerTest, ClientDisconnectDoesNotCrashServer) {
    TestClient c; c.connectTcp(); c.sendHandshake(30);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    c.close();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(nm->isRunning());
}

TEST_F(NetworkManagerTest, BroadcastWithNoClientsDoesNotCrash) {
    Chunk chunk;
    chunk.setCell(0, 0, {true, 1});
    WorldSerializer::ChunkList list{{{0, 0}, &chunk}};
    auto update = WorldSerializer::serializeDelta(1, list);
    EXPECT_NO_THROW(nm->broadcastWorldUpdate(update));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}