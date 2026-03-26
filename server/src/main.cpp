#include "GameServer.hpp"
#include "NetworkManager.hpp"
#include "BoostNetworkManager.hpp"

#include <iostream>
#include <chrono>
#include <thread>

namespace multilife
{

    // dummy network manager that does no real io
    class DummyNetworkManager : public NetworkManager
    {
    public:
        void start(std::uint16_t port) override {
            std::cout << "DummyNetworkManager listening on port " << port << '\n';
        }

        void stop() override {
            std::cout << "DummyNetworkManager stopping\n";
        }

        void poll() override { }

        void broadcastWorldUpdate(const SerializedWorldUpdate& update) override {
            std::cout << "Broadcasting world update of size " << update.data.size() << " bytes\n";
        }

        void setCommandCallback(std::function<void(std::vector<PlayerCommand>)> callback) override {
            m_callback = std::move(callback);
        }

    private:
        std::function<void(std::vector<PlayerCommand>)> m_callback;
    };

} // namespace multilife

int main() {
    using namespace std::chrono_literals;

    auto networkManager = std::make_unique<multilife::BoostNetworkManager>();
    multilife::GameServer server(std::move(networkManager),
                                 /*workerThreads*/ 4,
                                 std::chrono::milliseconds{100});

    server.start(9000);

    while (server.isRunning()) {
        std::this_thread::sleep_for(100ms);
    }

    server.stop();
    std::cout << "Server stopped.\n";
    return 0;
}

