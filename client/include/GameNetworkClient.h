#pragma once

#include "WorldModel.h"

#include <QHostAddress>
#include <QObject>
#include <QVector>
#include <QTcpSocket>
#include <QTimer>
#include <QUdpSocket>

#include <cstdint>
#include <optional>

class GameNetworkClient final : public QObject
{
    Q_OBJECT

public:
    struct PlayerSnapshot {
        std::uint64_t playerId{0};
        std::uint64_t balance{0};
        std::uint64_t liveCells{0};
    };

    explicit GameNetworkClient(QObject* parent = nullptr);

    static void registerMetaTypes();

    [[nodiscard]] bool isConnected() const;

public slots:
    void connectToServer(const QString& host,
                         quint16 tcpPort,
                         quint16 udpPort,
                         quint64 playerId);
    void disconnectFromServer();

    void sendPlaceCell(std::int64_t x, std::int64_t y);
    void sendRemoveCell(std::int64_t x, std::int64_t y);
    void sendToggleCell(std::int64_t x, std::int64_t y);
    void sendResync();

signals:
    void connectionStateChanged(const QString& text, bool connected);
    void serverStatsUpdated(std::uint32_t generation,
                            const QVector<GameNetworkClient::PlayerSnapshot>& players);
    void worldUpdateReceived(std::uint32_t seqNum,
                             bool fullSnapshot,
                             std::int32_t chunkX,
                             std::int32_t chunkY,
                             const QVector<AliveCell>& cells);

private slots:
    void onTcpConnected();
    void onTcpDisconnected();
    void onTcpError(QAbstractSocket::SocketError socketError);
    void onTcpReadyRead();
    void onUdpReadyRead();
    void reconnectTick();

private:
    template<typename T>
    static T readLE(const char* data);

    template<typename T>
    static void appendLE(QByteArray& out, T value);

    void sendCommand(std::uint8_t type, std::int64_t x, std::int64_t y);
    void scheduleReconnect();
    void cancelReconnect();
    void applyDatagram(const QByteArray& datagram);

    QTcpSocket* m_tcpSocket{nullptr};
    QUdpSocket* m_udpSocket{nullptr};
    QTimer* m_reconnectTimer{nullptr};

    QString m_host;
    quint16 m_tcpPort{0};
    quint16 m_udpPort{0};
    std::uint64_t m_playerId{0};
    bool m_manualDisconnect{false};
    std::optional<std::uint32_t> m_lastSeqNum;
    QByteArray m_tcpReadBuffer;
};

Q_DECLARE_METATYPE(GameNetworkClient::PlayerSnapshot)
