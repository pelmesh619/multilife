#include "ClientRuntime.h"

#include <QMetaObject>

ClientRuntime::ClientRuntime(QObject* parent)
    : QObject(parent)
    , m_client(new GameNetworkClient())
{
    GameNetworkClient::registerMetaTypes();

    m_client->moveToThread(&m_networkThread);
    connect(&m_networkThread, &QThread::finished, m_client, &QObject::deleteLater);

    connect(m_client,
            &GameNetworkClient::connectionStateChanged,
            this,
            &ClientRuntime::connectionStateChanged,
            Qt::QueuedConnection);
    connect(m_client,
            &GameNetworkClient::serverStatsUpdated,
            this,
            &ClientRuntime::serverStatsUpdated,
            Qt::QueuedConnection);
    connect(m_client,
            &GameNetworkClient::worldUpdateReceived,
            this,
            &ClientRuntime::worldUpdateReceived,
            Qt::QueuedConnection);

    m_networkThread.start();
}

ClientRuntime::~ClientRuntime()
{
    if (m_client) {
        QMetaObject::invokeMethod(m_client, "disconnectFromServer", Qt::BlockingQueuedConnection);
    }
    m_networkThread.quit();
    m_networkThread.wait();
    m_client = nullptr;
}

void ClientRuntime::connectToServer(const QString& host,
                                    quint16 tcpPort,
                                    quint16 udpPort,
                                    quint64 playerId)
{
    QMetaObject::invokeMethod(m_client,
                              "connectToServer",
                              Qt::QueuedConnection,
                              Q_ARG(QString, host),
                              Q_ARG(quint16, tcpPort),
                              Q_ARG(quint16, udpPort),
                              Q_ARG(quint64, playerId));
}

void ClientRuntime::disconnectFromServer()
{
    QMetaObject::invokeMethod(m_client, "disconnectFromServer", Qt::QueuedConnection);
}

void ClientRuntime::sendPlaceCell(std::int64_t x, std::int64_t y)
{
    QMetaObject::invokeMethod(
        m_client, "sendPlaceCell", Qt::QueuedConnection, Q_ARG(std::int64_t, x), Q_ARG(std::int64_t, y));
}

void ClientRuntime::sendRemoveCell(std::int64_t x, std::int64_t y)
{
    QMetaObject::invokeMethod(
        m_client, "sendRemoveCell", Qt::QueuedConnection, Q_ARG(std::int64_t, x), Q_ARG(std::int64_t, y));
}

void ClientRuntime::sendToggleCell(std::int64_t x, std::int64_t y)
{
    QMetaObject::invokeMethod(
        m_client, "sendToggleCell", Qt::QueuedConnection, Q_ARG(std::int64_t, x), Q_ARG(std::int64_t, y));
}

void ClientRuntime::sendResync()
{
    QMetaObject::invokeMethod(m_client, "sendResync", Qt::QueuedConnection);
}
