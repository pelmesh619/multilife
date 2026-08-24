#include <gtest/gtest.h>
#include "GameServer.hpp"
#include "NetworkManager.hpp"
#include "PlayerCommand.hpp"
#include "Types.hpp"

#include <chrono>
#include <thread>
#include <atomic>
#include <vector>
#include <iostream>

using namespace multilife;

// Stub NetworkManager

class StubNetworkManager : public NetworkManager {
public:
    std::function<void(std::vector<PlayerCommand>)> m_callback;
    std::function<void(PlayerId)> m_callback2;
    std::atomic<int> broadcastCount{0};

    void start(std::uint16_t, std::uint16_t) override {}
    void stop() override {}
    void poll() override {}

    void broadcastWorldUpdate(const SerializedWorldUpdate& update) override {
        ++broadcastCount;
        lastUpdateSize = update.data.size();
    }
    void broadcastServerStats(const std::vector<std::uint8_t>& payload) override {
        lastStatsSize = payload.size();
    }

    void setCommandCallback(std::function<void(std::vector<PlayerCommand>)> cb) override {
        m_callback = std::move(cb);
    }
    void setAddPlayerCallback(std::function<void(PlayerId)> cb) override {
        m_callback2 = std::move(cb);
    }

    void injectCommands(std::vector<PlayerCommand> cmds) {
        if (m_callback) m_callback(std::move(cmds));
    }

    std::size_t lastUpdateSize = 0;
    std::size_t lastStatsSize = 0;
};

// Helper functions
static std::pair<GameServer*, StubNetworkManager*> makeServer(
    int workers = 2,
    std::chrono::milliseconds tickInterval = std::chrono::milliseconds(50))
{
    auto* stub = new StubNetworkManager();
    auto server = new GameServer(
        std::unique_ptr<NetworkManager>(stub),
        workers,
        tickInterval
    );
    server->networkManager().setAddPlayerCallback([&](multilife::PlayerId playerId) {
        std::cout << "Add balance for " << playerId << '\n';
        server->resources().addPlayer(playerId);
    });
    return {server, stub};
}

template <typename Pred>
static bool waitUntil(Pred pred, std::chrono::milliseconds timeout = std::chrono::milliseconds(2000))
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!pred()) {
        if (std::chrono::steady_clock::now() >= deadline) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return true;
}

static bool isHorizontalBlinker(const Chunk* chunk, std::size_t x, std::size_t y)
{
    return chunk != nullptr
        && chunk->getCell(x - 1, y).alive
        && chunk->getCell(x, y).alive
        && chunk->getCell(x + 1, y).alive
        && !chunk->getCell(x, y - 1).alive
        && !chunk->getCell(x, y + 1).alive;
}

static bool isVerticalBlinker(const Chunk* chunk, std::size_t x, std::size_t y)
{
    return chunk != nullptr
        && chunk->getCell(x, y - 1).alive
        && chunk->getCell(x, y).alive
        && chunk->getCell(x, y + 1).alive
        && !chunk->getCell(x - 1, y).alive
        && !chunk->getCell(x + 1, y).alive;
}

// Lifecycle tests

TEST(GameServerTest, StartAndStopWithoutError) {
    auto [server, stub] = makeServer();
    EXPECT_NO_THROW(server->start(0, 0));
    EXPECT_TRUE(server->isRunning());
    EXPECT_NO_THROW(server->stop());
    EXPECT_FALSE(server->isRunning());
    delete server;
}

TEST(GameServerTest, StopBeforeStartIsHarmless) {
    auto [server, stub] = makeServer();
    EXPECT_NO_THROW(server->stop());
    delete server;
}

// Command pipeline tests

TEST(GameServerTest, CommandsFromNetworkReachWorld) {
    auto [server, stub] = makeServer();
    server->start(0, 0);

    stub->injectCommands({{1, CommandType::PlaceCell, 5, 5}});

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    server->stop();

    // The chunk must exist
    const Chunk* chunk = server->world().tryGetChunk({0, 0});
    EXPECT_NE(chunk, nullptr);

    delete server;
}

TEST(GameServerTest, MultipleCommandsFromNetworkAllApplied) {
    auto [server, stub] = makeServer(2, std::chrono::milliseconds(500));
    server->start(0, 0);
    server->resources().addPlayer(1);
    server->resources().addPlayer(2);
    server->resources().addPlayer(3);
    server->resources().addPlayer(4);

    // 2x2 block
    stub->injectCommands({
        {1, CommandType::PlaceCell, 10, 10},
        {2, CommandType::PlaceCell, 11, 10},
        {3, CommandType::PlaceCell, 10, 11},
        {4, CommandType::PlaceCell, 11, 11},
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    server->stop();

    const Chunk* chunk = server->world().tryGetChunk({0, 0});
    ASSERT_NE(chunk, nullptr);
    EXPECT_TRUE(chunk->getCell(10, 10).alive);
    EXPECT_TRUE(chunk->getCell(11, 10).alive);
    EXPECT_TRUE(chunk->getCell(10, 11).alive);
    EXPECT_TRUE(chunk->getCell(11, 11).alive);
    EXPECT_EQ(chunk->getCell(10, 10).owner, 1u);
    EXPECT_EQ(chunk->getCell(11, 10).owner, 2u);
    EXPECT_EQ(chunk->getCell(10, 11).owner, 3u);
    EXPECT_EQ(chunk->getCell(11, 11).owner, 4u);

    delete server;
}

// Resource distribution via tick

TEST(GameServerTest, LiveCellsAwardResourcesOverTime) {
    auto [server, stub] = makeServer(2, std::chrono::milliseconds(50));
    server->start(0, 0);
    server->resources().addPlayer(1);

    // 2x2 block for player 1
    stub->injectCommands({
        {1, CommandType::PlaceCell, 10, 10},
        {1, CommandType::PlaceCell, 11, 10},
        {1, CommandType::PlaceCell, 10, 11},
        {1, CommandType::PlaceCell, 11, 11},
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    server->stop();

    // ~8 ticks * 4 cells >= 32 resources
    EXPECT_GT(server->resources().getBalance(1), 32u);

    delete server;
}

// Tick simulation correctness

TEST(GameServerTest, BlinkerOscillatesOverTicks) {
    auto [server, stub] = makeServer(2, std::chrono::milliseconds(50));
    server->start(0, 0);
    server->resources().addPlayer(1);

    stub->injectCommands({
        {1, CommandType::PlaceCell, 5, 5},
        {1, CommandType::PlaceCell, 6, 5},
        {1, CommandType::PlaceCell, 7, 5},
    });

    auto chunk00 = [&]() { return server->world().tryGetChunk({0, 0}); };

    // Commands are applied after a simulation step, so wait for the placed
    // horizontal blinker rather than assuming a fixed number of ticks.
    ASSERT_TRUE(waitUntil([&] { return isHorizontalBlinker(chunk00(), 6, 5); }));
    ASSERT_TRUE(waitUntil([&] { return isVerticalBlinker(chunk00(), 6, 5); }));
    ASSERT_TRUE(waitUntil([&] { return isHorizontalBlinker(chunk00(), 6, 5); }));

    server->stop();
    delete server;
}

TEST(GameServerTest, BlinkerAcrossChunkBoundaryOscillatesOverTicks) {
    auto [server, stub] = makeServer(2, std::chrono::milliseconds(50));
    server->start(0, 0);
    server->resources().addPlayer(1);

    stub->injectCommands({
        {1, CommandType::PlaceCell, 1, 0},
        {1, CommandType::PlaceCell, 2, 0},
        {1, CommandType::PlaceCell, 3, 0},
    });

    auto isHorizontal = [&]() {
        const Chunk* baseChunk = server->world().tryGetChunk({0, 0});
        const Chunk* northChunk = server->world().tryGetChunk({0, -1});
        return baseChunk != nullptr
            && baseChunk->getCell(1, 0).alive
            && baseChunk->getCell(2, 0).alive
            && baseChunk->getCell(3, 0).alive
            && !baseChunk->getCell(2, 1).alive
            && (northChunk == nullptr || !northChunk->getCell(2, ChunkHeight - 1).alive);
    };
    auto isVertical = [&]() {
        const Chunk* northChunk = server->world().tryGetChunk({0, -1});
        const Chunk* baseChunk = server->world().tryGetChunk({0, 0});
        return northChunk != nullptr
            && baseChunk != nullptr
            && northChunk->getCell(2, ChunkHeight - 1).alive
            && baseChunk->getCell(2, 0).alive
            && baseChunk->getCell(2, 1).alive
            && !baseChunk->getCell(1, 0).alive
            && !baseChunk->getCell(3, 0).alive;
    };

    ASSERT_TRUE(waitUntil(isHorizontal));
    ASSERT_TRUE(waitUntil(isVertical));
    ASSERT_TRUE(waitUntil(isHorizontal));

    server->stop();
    delete server;
}

// Accessors

TEST(GameServerTest, WorldAccessorReturnsConsistentReference) {
    auto [server, stub] = makeServer();
    World& w1 = server->world();
    World& w2 = server->world();
    EXPECT_EQ(&w1, &w2);
    delete server;
}

TEST(GameServerTest, ResourcesAccessorReturnsConsistentReference) {
    auto [server, stub] = makeServer();
    ResourceManager& r1 = server->resources();
    ResourceManager& r2 = server->resources();
    EXPECT_EQ(&r1, &r2);
    delete server;
}
