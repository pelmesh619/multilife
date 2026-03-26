#include "BoostNetworkManager.hpp"
#include "PlayerCommand.hpp"

#include <cstring>
#include <iostream>

namespace multilife {

// Endian helper function
namespace {

template<typename T>
T readLE(const std::uint8_t* p) {
    T v{};
    std::memcpy(&v, p, sizeof(T));
    return v;
}

template<typename T>
void writeLE(std::uint8_t* p, T v) {
    std::memcpy(p, &v, sizeof(T));
}

} // namespace


BoostNetworkManager::BoostNetworkManager()
    : m_work(boost::asio::make_work_guard(m_ioc))
    , m_acceptor(m_ioc)
    , m_udpSocket(m_ioc)
{}

BoostNetworkManager::~BoostNetworkManager() {
    stop();
}

void BoostNetworkManager::setCommandCallback(std::function<void(std::vector<PlayerCommand>)> cb) {
    m_commandCallback = std::move(cb);
}

void BoostNetworkManager::setFullSnapshotProvider(FullSnapshotProvider p) {
    m_fullSnapshotProvider = std::move(p);
}


void BoostNetworkManager::start(std::uint16_t port) {
    if (m_running.exchange(true)) return;

    boost::asio::ip::tcp::endpoint tcpEp(boost::asio::ip::tcp::v4(), port);
    m_acceptor.open(tcpEp.protocol());
    m_acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
    m_acceptor.bind(tcpEp);
    m_acceptor.listen();

    boost::asio::ip::udp::endpoint udpEp(boost::asio::ip::udp::v4(), port);
    m_udpSocket.open(udpEp.protocol());
    m_udpSocket.bind(udpEp);

    doAccept();

    m_ioThread = std::thread([this] {
        try { m_ioc.run(); }
        catch (const std::exception& e) {
            std::cerr << "[BoostNetworkManager] io_context: " << e.what() << '\n';
        }
    });

    std::cout << "[BoostNetworkManager] Listening on port " << port << '\n';
}

void BoostNetworkManager::stop() {
    if (!m_running.exchange(false)) return;

    m_work.reset();

    boost::system::error_code ec;
    m_acceptor.close(ec);
    m_udpSocket.close(ec);

    {
        std::lock_guard<std::mutex> lk(m_sessionsMutex);
        for (auto& [id, s] : m_sessions) {
            boost::system::error_code sec;
            s->socket.close(sec);
        }
        m_sessions.clear();
        m_udpEndpoints.clear();
    }

    if (m_ioThread.joinable()) m_ioThread.join();
    std::cout << "[BoostNetworkManager] Stopped.\n";
}


void BoostNetworkManager::doAccept()
{
    m_acceptor.async_accept(
        [this](boost::system::error_code ec,
               boost::asio::ip::tcp::socket socket)
        {
            if (!m_running) return;
            if (!ec) {
                std::make_shared<Session>(std::move(socket), *this)->start();
            } else if (ec != boost::asio::error::operation_aborted) {
                std::cerr << "[BoostNetworkManager] Accept: " << ec.message() << '\n';
            }
            if (m_running) doAccept();
        });
}


void BoostNetworkManager::broadcastWorldUpdate(const SerializedWorldUpdate& update)
{
    if (update.data.empty()) return;

    const std::uint32_t seq = ++m_seqNum;

    boost::asio::post(m_ioc,
        [this, payload = update.data, seq]() mutable {
            doUdpBroadcast(std::move(payload), seq);
        });
}

void BoostNetworkManager::doUdpBroadcast(
    std::vector<std::uint8_t> deltaData,
    std::uint32_t             seqNum) {
    struct SessionInfo {
        PlayerId                       id;
        boost::asio::ip::udp::endpoint ep;
        bool                           needsFull;
    };
    std::vector<SessionInfo> infos;
    {
        std::lock_guard<std::mutex> lk(m_sessionsMutex);
        for (auto& [id, session] : m_sessions) {
            auto epIt = m_udpEndpoints.find(id);
            if (epIt == m_udpEndpoints.end()) continue;
            infos.push_back({id, epIt->second, session->needsFullSnapshot});
        }
    }

    if (infos.empty()) return;

    std::vector<std::uint8_t> fullData;
    bool fullBuilt = false;

    for (auto& info : infos) {
        if (info.needsFull) {
            // build the full snapshot on first need
            if (!fullBuilt && m_fullSnapshotProvider) {
                auto snap = m_fullSnapshotProvider(seqNum);
                fullData  = std::move(snap.data);
                fullBuilt = true;
            }
            sendPayloadTo(fullData, info.ep);

            std::lock_guard<std::mutex> lk(m_sessionsMutex);
            auto it = m_sessions.find(info.id);
            if (it != m_sessions.end())
                it->second->needsFullSnapshot = false;
        } else {
            sendPayloadTo(deltaData, info.ep);
        }
    }
}

void BoostNetworkManager::sendPayloadTo(
    const std::vector<std::uint8_t>&      payload,
    const boost::asio::ip::udp::endpoint& ep) {
    if (payload.empty()) return;

    const std::uint8_t* ptr = payload.data();
    const std::uint8_t* end = ptr + payload.size();

    while (ptr + proto::kUdpHeader <= end) {
        std::uint16_t cellCount = readLE<std::uint16_t>(ptr + proto::kOffCellCount);

        std::size_t pktSize =  proto::kUdpHeader + cellCount * proto::kUdpCellEntry;

        if (pktSize > proto::kMaxUdpPayload) {
            const std::uint16_t maxCells = static_cast<std::uint16_t>(proto::kMaxCellsPerPacket);
            std::vector<std::uint8_t> capped(
                ptr,
                ptr + proto::kUdpHeader + maxCells * proto::kUdpCellEntry
            );
            writeLE<std::uint16_t>(capped.data() + proto::kOffCellCount, maxCells);

            boost::system::error_code ec;
            m_udpSocket.send_to(boost::asio::buffer(capped), ep, 0, ec);

            ptr += proto::kUdpHeader + cellCount * proto::kUdpCellEntry;
        } else {
            boost::system::error_code ec;
            m_udpSocket.send_to(
                boost::asio::buffer(ptr, pktSize), ep, 0, ec);
            ptr += pktSize;
        }
    }
}

void BoostNetworkManager::removeSession(PlayerId id) {
    std::lock_guard<std::mutex> lk(m_sessionsMutex);
    m_sessions.erase(id);
    m_udpEndpoints.erase(id);
    std::cout << "[BoostNetworkManager] Player " << id << " disconnected.\n";
}


BoostNetworkManager::Session::Session(
    boost::asio::ip::tcp::socket s,
    BoostNetworkManager&         o)
    : socket(std::move(s))
    , owner(o)
{}

void BoostNetworkManager::Session::start() {
    boost::asio::async_read(
        socket,
        boost::asio::buffer(handshakeBuf),
        [self = shared_from_this()](
            boost::system::error_code ec, std::size_t)
        {
            if (ec) {
                std::cerr << "[Session] Handshake read error: "
                          << ec.message() << '\n';
                return;
            }

            const std::uint32_t magic = readLE<std::uint32_t>(self->handshakeBuf.data());
            if (magic != proto::kMagic) {
                std::cerr << "[Session] Bad magic 0x" << std::hex << magic
                          << std::dec << " — closing.\n";
                boost::system::error_code sec;
                self->socket.close(sec);
                return;
            }

            self->playerId = readLE<std::uint64_t>(self->handshakeBuf.data() + 4);
            self->handshakeDone = true;
            self->needsFullSnapshot = true;  // first message must be a full snap

            std::cout << "[BoostNetworkManager] Player " << self->playerId
                      << " connected from "
                      << self->socket.remote_endpoint() << '\n';

            {
                std::lock_guard<std::mutex> lk(self->owner.m_sessionsMutex);
                self->owner.m_sessions[self->playerId] = self;

                auto peer = self->socket.remote_endpoint();
                self->owner.m_udpEndpoints[self->playerId] =
                    boost::asio::ip::udp::endpoint(peer.address(), peer.port());
            }

            self->readNextMessage();
        });
}

void BoostNetworkManager::Session::readNextMessage()
{
    // Read the 1-byte message-type prefix first, then dispatch.
    boost::asio::async_read(
        socket,
        boost::asio::buffer(msgTypeBuf),
        [self = shared_from_this()](
            boost::system::error_code ec, std::size_t)
        {
            if (ec) {
                if (ec != boost::asio::error::eof &&
                    ec != boost::asio::error::connection_reset)
                    std::cerr << "[Session] Read error: " << ec.message() << '\n';
                self->owner.removeSession(self->playerId);
                return;
            }

            const std::uint8_t msgType = self->msgTypeBuf[0];

            // Resync request
            if (msgType == proto::kMsgResyncReq) {
                std::cout << "[BoostNetworkManager] Player "
                          << self->playerId << " requested resync.\n";
                {
                    std::lock_guard<std::mutex> lk(self->owner.m_sessionsMutex);
                    auto it = self->owner.m_sessions.find(self->playerId);
                    if (it != self->owner.m_sessions.end())
                        it->second->needsFullSnapshot = true;
                }
                self->readNextMessage();
                return;
            }

            // PlayerCommand
            if (msgType == proto::kCmdPlace  ||
                msgType == proto::kCmdRemove ||
                msgType == proto::kCmdToggle)
            {
                boost::asio::async_read(
                    self->socket,
                    boost::asio::buffer(self->cmdBodyBuf),
                    [self, msgType](
                        boost::system::error_code ec2, std::size_t)
                    {
                        if (ec2) {
                            if (ec2 != boost::asio::error::eof &&
                                ec2 != boost::asio::error::connection_reset)
                                std::cerr << "[Session] Command body read: "
                                          << ec2.message() << '\n';
                            self->owner.removeSession(self->playerId);
                            return;
                        }

                        // [uint64 playerId][int64 x][int64 y]
                        const std::uint8_t* p = self->cmdBodyBuf.data();
                        const std::uint64_t cmdPlayer =
                            readLE<std::uint64_t>(p);
                        const std::int64_t  x =
                            readLE<std::int64_t>(p + 8);
                        const std::int64_t  y =
                            readLE<std::int64_t>(p + 16);

                        if (cmdPlayer != self->playerId) {
                            std::cerr << "[Session] Player "
                                      << self->playerId
                                      << " spoofed player "
                                      << cmdPlayer << " — ignored.\n";
                            self->readNextMessage();
                            return;
                        }

                        CommandType type;
                        switch (msgType) {
                        case proto::kCmdPlace:  type = CommandType::PlaceCell;  break;
                        case proto::kCmdRemove: type = CommandType::RemoveCell; break;
                        default:                type = CommandType::ToggleCell; break;
                        }

                        if (self->owner.m_commandCallback)
                            self->owner.m_commandCallback(
                                {{self->playerId, type, x, y}});

                        self->readNextMessage();
                    });
                return;
            }


            std::cerr << "[Session] Unknown msg type 0x"
                      << std::hex << static_cast<int>(msgType)
                      << std::dec << " — ignored.\n";
            self->readNextMessage();
        });
}

} // namespace multilife