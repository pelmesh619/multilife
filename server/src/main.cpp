#include "GameServer.hpp"
#include "BoostNetworkManager.hpp"
#include "Types.hpp"

#include <iostream>
#include <chrono>
#include <thread>

int main() {
    using namespace std::chrono_literals;

    constexpr std::uint16_t kTcpPort = 9000;
    constexpr std::uint16_t kUdpPort = 9001;
    constexpr std::size_t kWorkerThreads = 4;
    constexpr auto kTickInterval = 5s;

    auto networkManager = std::make_unique<multilife::BoostNetworkManager>();
    multilife::GameServer server(std::move(networkManager), kWorkerThreads, kTickInterval);

    server.networkManager().setAddPlayerCallback([&](multilife::PlayerId playerId) {
        std::cout << "Player " << playerId << " joined\n";
        server.resources().addPlayer(playerId);
    });

    server.start(kTcpPort, kUdpPort);
    std::cout << "Server listening on TCP " << kTcpPort << " / UDP " << kUdpPort << '\n';

    while (server.isRunning()) {
        std::this_thread::sleep_for(100ms);
    }

    server.stop();
    std::cout << "Server stopped.\n";
    return 0;
}
