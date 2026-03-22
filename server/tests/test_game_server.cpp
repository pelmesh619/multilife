#include <gtest/gtest.h>
#include "GameServer.hpp"
#include "NetworkManager.hpp"
#include "PlayerCommand.hpp"
#include "Types.hpp"

#include <chrono>
#include <thread>
#include <atomic>
#include <vector>

using namespace multilife;

// Stub NetworkManager

class StubNetworkManager : public NetworkManager {
public:
    std::function<void(std::vector<PlayerCommand>)> m_callback;
    std::atomic<int> broadcastCount{0};

    void start(std::uint16_t) override {}
    void stop() override {}
    void poll() override {}

    void broadcastWorldUpdate(const SerializedWorldUpdate& update) override {
        ++broadcastCount;
        lastUpdateSize = update.data.size();
    }

    void setCommandCallback(std::function<void(std::vector<PlayerCommand>)> cb) override {
        m_callback = std::move(cb);
    }

    void injectCommands(std::vector<PlayerCommand> cmds) {
        if (m_callback) m_callback(std::move(cmds));
    }

    std::size_t lastUpdateSize = 0;
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
    return {server, stub};
}

// Lifecycle tests

TEST(GameServerTest, StartAndStopWithoutError) {
    auto [server, stub] = makeServer();
    EXPECT_NO_THROW(server->start(0));
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
    server->start(0);

    stub->injectCommands({{1, CommandType::PlaceCell, 5, 5}});

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    server->stop();

    // The chunk must exist
    const Chunk* chunk = server->world().tryGetChunk({0, 0});
    EXPECT_NE(chunk, nullptr);

    delete server;
}

// Resource distribution via tick

TEST(GameServerTest, LiveCellsAwardResourcesOverTime) {
    auto [server, stub] = makeServer(2, std::chrono::milliseconds(50));
    server->start(0);

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
    auto [server, stub] = makeServer(2, std::chrono::milliseconds(100));
    server->start(0);

    // Horizontal blinker
    stub->injectCommands({
        {1, CommandType::PlaceCell, 5, 5},
        {1, CommandType::PlaceCell, 6, 5},
        {1, CommandType::PlaceCell, 7, 5},
    });

    // Tick 1 applies commands, tick 2 simulates -> vertical blinker.
    std::this_thread::sleep_for(std::chrono::milliseconds(250)); // ~2 ticks

    {
        const Chunk* chunk = server->world().tryGetChunk({0, 0});
        ASSERT_NE(chunk, nullptr);
        EXPECT_TRUE(chunk->getCell(6, 4).alive);
        EXPECT_TRUE(chunk->getCell(6, 5).alive);
        EXPECT_TRUE(chunk->getCell(6, 6).alive);
        EXPECT_FALSE(chunk->getCell(5, 5).alive);
        EXPECT_FALSE(chunk->getCell(7, 5).alive);
    }

    // 1 tick
    std::this_thread::sleep_for(std::chrono::milliseconds(110));
    server->stop();

    {
        const Chunk* chunk = server->world().tryGetChunk({0, 0});
        ASSERT_NE(chunk, nullptr);
        EXPECT_TRUE(chunk->getCell(5, 5).alive);
        EXPECT_TRUE(chunk->getCell(6, 5).alive);
        EXPECT_TRUE(chunk->getCell(7, 5).alive);
    }

    delete server;
}
