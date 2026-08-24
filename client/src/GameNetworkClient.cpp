#include "GameNetworkClient.h"

#include "ClientParsers.h"
#include "ClientProtocol.h"

#include <QByteArray>
#include <QMetaType>
#include <QNetworkDatagram>

#include <cstring>

void GameNetworkClient::registerMetaTypes()
{
    qRegisterMetaType<AliveCell>();
    qRegisterMetaType<QVector<AliveCell>>();
    qRegisterMetaType<GameNetworkClient::PlayerSnapshot>();
    qRegisterMetaType<QVector<GameNetworkClient::PlayerSnapshot>>();
}

GameNetworkClient::GameNetworkClient(QObject* parent)
    : QObject(parent)
{
    m_tcpSocket = new QTcpSocket(this);
    m_udpSocket = new QUdpSocket(this);
    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setInterval(2000);
    m_reconnectTimer->setSingleShot(false);

    connect(m_tcpSocket, &QTcpSocket::connected, this, &GameNetworkClient::onTcpConnected);
    connect(m_tcpSocket, &QTcpSocket::disconnected, this, &GameNetworkClient::onTcpDisconnected);
    connect(m_tcpSocket,
            &QTcpSocket::errorOccurred,
            this,
            &GameNetworkClient::onTcpError);
    connect(m_tcpSocket, &QTcpSocket::readyRead, this, &GameNetworkClient::onTcpReadyRead);
    connect(m_udpSocket, &QUdpSocket::readyRead, this, &GameNetworkClient::onUdpReadyRead);
    connect(m_reconnectTimer, &QTimer::timeout, this, &GameNetworkClient::reconnectTick);
}

void GameNetworkClient::connectToServer(const QString& host,
                                        quint16 tcpPort,
                                        quint16 udpPort,
                                        quint64 playerId)
{
    m_host = host;
    m_tcpPort = tcpPort;
    m_udpPort = udpPort;
    m_playerId = playerId;
    m_manualDisconnect = false;
    m_lastSeqNum.reset();
    m_tcpReadBuffer.clear();

    if (m_udpSocket->state() != QAbstractSocket::UnconnectedState) {
        m_udpSocket->close();
    }
    if (m_tcpSocket->state() != QAbstractSocket::UnconnectedState) {
        m_tcpSocket->abort();
    }

    emit connectionStateChanged(
        QString("Connecting to %1:%2 ...").arg(m_host).arg(m_tcpPort),
        false);
    m_tcpSocket->connectToHost(m_host, m_tcpPort);
}

void GameNetworkClient::disconnectFromServer()
{
    m_manualDisconnect = true;
    cancelReconnect();
    m_lastSeqNum.reset();
    m_tcpReadBuffer.clear();
    m_udpSocket->close();
    m_tcpSocket->disconnectFromHost();
    if (m_tcpSocket->state() != QAbstractSocket::UnconnectedState) {
        m_tcpSocket->abort();
    }
    emit connectionStateChanged("Disconnected", false);
}

void GameNetworkClient::sendPlaceCell(std::int64_t x, std::int64_t y)
{
    sendCommand(multilife::client::proto::kCmdPlace, x, y);
}

void GameNetworkClient::sendRemoveCell(std::int64_t x, std::int64_t y)
{
    sendCommand(multilife::client::proto::kCmdRemove, x, y);
}

void GameNetworkClient::sendToggleCell(std::int64_t x, std::int64_t y)
{
    sendCommand(multilife::client::proto::kCmdToggle, x, y);
}

void GameNetworkClient::sendResync()
{
    if (!isConnected()) {
        return;
    }
    QByteArray request;
    request.append(static_cast<char>(multilife::client::proto::kMsgResyncReq));
    m_tcpSocket->write(request);
}

bool GameNetworkClient::isConnected() const
{
    return m_tcpSocket->state() == QAbstractSocket::ConnectedState;
}

void GameNetworkClient::onTcpConnected()
{
    cancelReconnect();

    QByteArray handshake;
    handshake.resize(static_cast<int>(multilife::client::proto::kHandshakeSize));
    std::memcpy(handshake.data(), &multilife::client::proto::kMagic, sizeof(std::uint32_t));
    std::memcpy(handshake.data() + 4, &m_playerId, sizeof(std::uint64_t));
    m_tcpSocket->write(handshake);

    const auto localPort = m_tcpSocket->localPort();
    if (!m_udpSocket->bind(QHostAddress::AnyIPv4, localPort, QUdpSocket::ShareAddress)) {
        emit connectionStateChanged(
            QString("TCP connected, UDP bind failed (%1)")
                .arg(m_udpSocket->errorString()),
            false);
        scheduleReconnect();
        return;
    }

    emit connectionStateChanged(
        QString("Connected as player %1 (UDP %2)").arg(m_playerId).arg(localPort),
        true);
}

void GameNetworkClient::onTcpDisconnected()
{
    m_udpSocket->close();
    m_tcpReadBuffer.clear();
    if (!m_manualDisconnect) {
        emit connectionStateChanged("Connection lost, trying to reconnect ...", false);
        scheduleReconnect();
    }
}

void GameNetworkClient::onTcpError(QAbstractSocket::SocketError socketError)
{
    if (socketError == QAbstractSocket::RemoteHostClosedError) {
        return;
    }

    emit connectionStateChanged(QString("Network error: %1").arg(m_tcpSocket->errorString()), false);
    if (!m_manualDisconnect) {
        scheduleReconnect();
    }
}

void GameNetworkClient::onUdpReadyRead()
{
    while (m_udpSocket->hasPendingDatagrams()) {
        const QNetworkDatagram datagram = m_udpSocket->receiveDatagram();
        applyDatagram(datagram.data());
    }
}

void GameNetworkClient::onTcpReadyRead()
{
    m_tcpReadBuffer.append(m_tcpSocket->readAll());

    while (true) {
        const auto parsed = multilife::client::parse::tryParseServerStatsMessage(m_tcpReadBuffer);
        if (!parsed.has_value()) {
            return;
        }

        QVector<PlayerSnapshot> players;
        players.reserve(parsed->players.size());
        for (const auto& p : parsed->players) {
            players.push_back(PlayerSnapshot{p.playerId, p.balance, p.liveCells});
        }
        emit serverStatsUpdated(parsed->generation, players);
    }
}

void GameNetworkClient::reconnectTick()
{
    if (isConnected()) {
        cancelReconnect();
        return;
    }
    if (m_tcpSocket->state() != QAbstractSocket::UnconnectedState) {
        m_tcpSocket->abort();
    }
    m_tcpSocket->connectToHost(m_host, m_tcpPort);
}

template<typename T>
T GameNetworkClient::readLE(const char* data)
{
    T value{};
    std::memcpy(&value, data, sizeof(T));
    return value;
}

template<typename T>
void GameNetworkClient::appendLE(QByteArray& out, T value)
{
    const auto oldSize = out.size();
    out.resize(oldSize + static_cast<int>(sizeof(T)));
    std::memcpy(out.data() + oldSize, &value, sizeof(T));
}

void GameNetworkClient::sendCommand(std::uint8_t type, std::int64_t x, std::int64_t y)
{
    if (!isConnected()) {
        return;
    }

    QByteArray packet;
    packet.reserve(static_cast<int>(multilife::client::proto::kCommandSize));
    packet.append(static_cast<char>(type));
    appendLE(packet, m_playerId);
    appendLE(packet, x);
    appendLE(packet, y);
    m_tcpSocket->write(packet);
}

void GameNetworkClient::scheduleReconnect()
{
    if (m_manualDisconnect || m_reconnectTimer->isActive()) {
        return;
    }
    m_reconnectTimer->start();
}

void GameNetworkClient::cancelReconnect()
{
    if (m_reconnectTimer->isActive()) {
        m_reconnectTimer->stop();
    }
}

void GameNetworkClient::applyDatagram(const QByteArray& datagram)
{
    const auto parsed = multilife::client::parse::parseUdpWorldPacket(datagram);
    if (!parsed.has_value()) {
        return;
    }

    const bool fullSnapshot = parsed->fullSnapshot;
    if (m_lastSeqNum.has_value() && !fullSnapshot) {
        const std::uint32_t expected = *m_lastSeqNum + 1;
        if (parsed->seqNum != expected) {
            sendResync();
        }
    }
    m_lastSeqNum = parsed->seqNum;

    QVector<AliveCell> updates;
    updates.reserve(parsed->cells.size());
    for (const auto& cell : parsed->cells) {
        updates.push_back(AliveCell{
            cell.localX,
            cell.localY,
            cell.alive ? cell.owner : 0
        });
    }

    emit worldUpdateReceived(
        parsed->seqNum,
        fullSnapshot,
        parsed->chunkX,
        parsed->chunkY,
        updates);
}
