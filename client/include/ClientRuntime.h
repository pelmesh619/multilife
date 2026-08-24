#pragma once

#include "GameNetworkClient.h"

#include <QObject>
#include <QThread>

class ClientRuntime final : public QObject
{
    Q_OBJECT

public:
    explicit ClientRuntime(QObject* parent = nullptr);
    ~ClientRuntime() override;

    void connectToServer(const QString& host, quint16 tcpPort, quint16 udpPort, quint64 playerId);
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

private:
    GameNetworkClient* m_client{nullptr};
    QThread m_networkThread;
};
