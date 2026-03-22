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
