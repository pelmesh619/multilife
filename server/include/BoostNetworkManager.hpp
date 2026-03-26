#pragma once

#include "NetworkManager.hpp"
#include "Protocol.hpp"
#include "Types.hpp"

#include <boost/asio.hpp>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

namespace multilife {

class BoostNetworkManager : public NetworkManager {
public:
    explicit BoostNetworkManager();
    ~BoostNetworkManager() override;

    void start(std::uint16_t port) override;
    void stop() override;
    void poll() override {}

    void broadcastWorldUpdate(const SerializedWorldUpdate& update) override;

    void setCommandCallback(
        std::function<void(std::vector<PlayerCommand>)> callback) override;

    using FullSnapshotProvider = std::function<SerializedWorldUpdate(std::uint32_t seqNum)>;
    void setFullSnapshotProvider(FullSnapshotProvider provider);

    bool isRunning() const { return m_running.load(); }

private:
    struct Session : std::enable_shared_from_this<Session> {
        explicit Session(boost::asio::ip::tcp::socket socket,
                         BoostNetworkManager&          owner);

        void start();
        void readNextMessage();

        boost::asio::ip::tcp::socket socket;
        BoostNetworkManager& owner;
        PlayerId playerId{0};
        bool handshakeDone{false};

        bool needsFullSnapshot{true};

        std::array<std::uint8_t, proto::kHandshakeSize> handshakeBuf{};
        std::array<std::uint8_t, 1>                     msgTypeBuf{};
        std::array<std::uint8_t, proto::kCommandSize - 1> cmdBodyBuf{};
    };

    using SessionPtr = std::shared_ptr<Session>;

    void doAccept();
    void removeSession(PlayerId id);

    void doUdpBroadcast(std::vector<std::uint8_t> deltaData,
                        std::uint32_t             seqNum);

    void sendPayloadTo(const std::vector<std::uint8_t>& payload,
                       const boost::asio::ip::udp::endpoint& ep);

    boost::asio::io_context m_ioc;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type>
                                     m_work;

    boost::asio::ip::tcp::acceptor m_acceptor;
    boost::asio::ip::udp::socket m_udpSocket;

    std::thread m_ioThread;

    std::mutex m_sessionsMutex;
    std::unordered_map<PlayerId, SessionPtr> m_sessions;
    std::unordered_map<PlayerId, boost::asio::ip::udp::endpoint> m_udpEndpoints;

    std::function<void(std::vector<PlayerCommand>)> m_commandCallback;
    FullSnapshotProvider m_fullSnapshotProvider;

    std::uint32_t m_seqNum{0};

    std::atomic<bool> m_running{false};
};

} // namespace multilife