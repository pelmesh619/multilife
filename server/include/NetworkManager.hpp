#pragma once

#include "PlayerCommand.hpp"
#include "Types.hpp"

#include <vector>
#include <string>
#include <functional>

namespace multilife
{

    struct SerializedWorldUpdate {
        std::vector<std::uint8_t> data;
    };

    class GameServer;

    class NetworkManager
    {
    public:
        virtual ~NetworkManager() = default;

        virtual void start(std::uint16_t tcpPort, std::uint16_t udpPort) = 0;

        virtual void stop() = 0;

        virtual void poll() = 0;

        virtual void broadcastWorldUpdate(const SerializedWorldUpdate& update) = 0;

        virtual void setCommandCallback(std::function<void(std::vector<PlayerCommand>)> callback) = 0;
        virtual void setAddPlayerCallback(std::function<void(PlayerId)> callback) = 0;
    };

} // namespace multilife

