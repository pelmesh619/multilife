#include "ClientProtocol.h"
#include "GameNetworkClient.h"

#include <QCoreApplication>
#include <QHostAddress>
#include <QMetaObject>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>
#include <QThread>
#include <QUdpSocket>

#include <cstdint>
#include <cstring>
#include <memory>

namespace {

template<typename T>
void appendLE(QByteArray& out, T value)
{
    const auto oldSize = out.size();
    out.resize(oldSize + static_cast<int>(sizeof(T)));
    std::memcpy(out.data() + oldSize, &value, sizeof(T));
}

QByteArray makeUdpDatagram(std::uint32_t seq,
                           std::uint8_t flags,
                           std::int32_t chunkX,
                           std::int32_t chunkY,
                           const QVector<AliveCell>& cells)
{
    QByteArray out;
    out.reserve(static_cast<int>(
        multilife::client::proto::kUdpHeader +
        cells.size() * static_cast<int>(multilife::client::proto::kUdpCellEntry)));

    appendLE(out, seq);
    out.append(static_cast<char>(flags));
    appendLE(out, chunkX);
    appendLE(out, chunkY);
    appendLE(out, static_cast<std::uint16_t>(cells.size()));

    for (const auto& cell : cells) {
        out.append(static_cast<char>(cell.x));
        out.append(static_cast<char>(cell.y));
        out.append(static_cast<char>(cell.owner != 0 ? 1 : 0));
        appendLE(out, cell.owner);
    }
    return out;
}

bool hasConnectedEvent(const QSignalSpy& spy)
{
    for (const auto& row : spy) {
        if (row.size() < 2) {
            continue;
        }
        if (row[1].toBool()) {
            return true;
        }
    }
    return false;
}

bool containsTextEvent(const QSignalSpy& spy, const QString& textPart, bool connected)
{
    for (const auto& row : spy) {
        if (row.size() < 2) {
            continue;
        }
        if (row[0].toString().contains(textPart) && row[1].toBool() == connected) {
            return true;
        }
    }
    return false;
}

struct ClientThreadGuard {
    GameNetworkClient* client{nullptr};
    QThread* thread{nullptr};

    ~ClientThreadGuard()
    {
        if (client) {
            QMetaObject::invokeMethod(client, "disconnectFromServer", Qt::BlockingQueuedConnection);
        }
        if (thread && thread->isRunning()) {
            thread->quit();
            thread->wait(2000);
        }
        delete client;
    }
};

} // namespace

class NetworkThreadingTest final : public QObject
{
    Q_OBJECT

private slots:
    void clientSignalsFlowAcrossThreads()
    {
        GameNetworkClient::registerMetaTypes();

        QThread networkThread;
        auto* client = new GameNetworkClient();
        client->moveToThread(&networkThread);
        networkThread.start();
        ClientThreadGuard cleanup{client, &networkThread};

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        QSignalSpy connectionSpy(client, &GameNetworkClient::connectionStateChanged);
        QSignalSpy worldSpy(client, &GameNetworkClient::worldUpdateReceived);
        QVERIFY(connectionSpy.isValid());
        QVERIFY(worldSpy.isValid());

        QThread* emitThread = nullptr;
        QThread* queuedReceiverThread = nullptr;

        connect(client,
                &GameNetworkClient::worldUpdateReceived,
                this,
                [&](std::uint32_t, bool, std::int32_t, std::int32_t, const QVector<AliveCell>&) {
                    emitThread = QThread::currentThread();
                },
                Qt::DirectConnection);
        connect(client,
                &GameNetworkClient::worldUpdateReceived,
                this,
                [&](std::uint32_t, bool, std::int32_t, std::int32_t, const QVector<AliveCell>&) {
                    queuedReceiverThread = QThread::currentThread();
                },
                Qt::QueuedConnection);

        const bool invoked = QMetaObject::invokeMethod(client,
                                                       "connectToServer",
                                                       Qt::QueuedConnection,
                                                       Q_ARG(QString, QStringLiteral("127.0.0.1")),
                                                       Q_ARG(quint16, server.serverPort()),
                                                       Q_ARG(quint16, 0),
                                                       Q_ARG(quint64, 42));
        QVERIFY(invoked);

        QTRY_VERIFY_WITH_TIMEOUT(server.hasPendingConnections(), 2000);
        std::unique_ptr<QTcpSocket> peer(server.nextPendingConnection());
        QVERIFY(peer != nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(hasConnectedEvent(connectionSpy), 2000);

        const quint16 clientUdpPort = peer->peerPort();
        QUdpSocket sender;
        const auto datagram = makeUdpDatagram(
            7,
            multilife::client::proto::kFlagDelta,
            1,
            -2,
            {{1, 2, 777}});

        const qint64 sent = sender.writeDatagram(datagram, QHostAddress::LocalHost, clientUdpPort);
        QCOMPARE(sent, static_cast<qint64>(datagram.size()));

        QTRY_COMPARE_WITH_TIMEOUT(worldSpy.count(), 1, 2000);

        QCOMPARE(emitThread, &networkThread);
        QCOMPARE(queuedReceiverThread, QCoreApplication::instance()->thread());
        QCOMPARE(worldSpy[0][0].toUInt(), static_cast<uint>(7));
        QCOMPARE(worldSpy[0][1].toBool(), false);
        QCOMPARE(worldSpy[0][2].toInt(), 1);
        QCOMPARE(worldSpy[0][3].toInt(), -2);

        const auto updates = worldSpy[0][4].value<QVector<AliveCell>>();
        QCOMPARE(updates.size(), 1);
        QCOMPARE(updates[0].x, static_cast<std::int64_t>(1));
        QCOMPARE(updates[0].y, static_cast<std::int64_t>(2));
        QCOMPARE(updates[0].owner, static_cast<std::uint64_t>(777));

        QVERIFY(networkThread.isRunning());
    }

    void reconnectAfterTcpDropEmitsOrderedStateChanges()
    {
        GameNetworkClient::registerMetaTypes();

        QThread networkThread;
        auto* client = new GameNetworkClient();
        client->moveToThread(&networkThread);
        networkThread.start();
        ClientThreadGuard cleanup{client, &networkThread};

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        QSignalSpy connectionSpy(client, &GameNetworkClient::connectionStateChanged);
        QVERIFY(connectionSpy.isValid());

        QVERIFY(QMetaObject::invokeMethod(client,
                                          "connectToServer",
                                          Qt::QueuedConnection,
                                          Q_ARG(QString, QStringLiteral("127.0.0.1")),
                                          Q_ARG(quint16, server.serverPort()),
                                          Q_ARG(quint16, 0),
                                          Q_ARG(quint64, 777)));

        QTRY_VERIFY_WITH_TIMEOUT(server.hasPendingConnections(), 2000);
        std::unique_ptr<QTcpSocket> firstPeer(server.nextPendingConnection());
        QVERIFY(firstPeer != nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(firstPeer->bytesAvailable() >= 12 || firstPeer->waitForReadyRead(50), 2000);
        firstPeer->readAll();

        firstPeer->disconnectFromHost();
        firstPeer->close();

        QTRY_VERIFY_WITH_TIMEOUT(server.hasPendingConnections(), 5000);
        std::unique_ptr<QTcpSocket> secondPeer(server.nextPendingConnection());
        QVERIFY(secondPeer != nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(secondPeer->bytesAvailable() >= 12 || secondPeer->waitForReadyRead(50), 2000);
        secondPeer->readAll();

        QTRY_VERIFY_WITH_TIMEOUT(
            containsTextEvent(connectionSpy, QStringLiteral("Connection lost"), false), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(
            containsTextEvent(connectionSpy, QStringLiteral("Connected as player"), true), 5000);

        QVERIFY(networkThread.isRunning());
    }

    void droppedSequenceTriggersResyncAndRecovery()
    {
        GameNetworkClient::registerMetaTypes();

        QThread networkThread;
        auto* client = new GameNetworkClient();
        client->moveToThread(&networkThread);
        networkThread.start();
        ClientThreadGuard cleanup{client, &networkThread};

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        QSignalSpy worldSpy(client, &GameNetworkClient::worldUpdateReceived);
        QVERIFY(worldSpy.isValid());

        QVERIFY(QMetaObject::invokeMethod(client,
                                          "connectToServer",
                                          Qt::QueuedConnection,
                                          Q_ARG(QString, QStringLiteral("127.0.0.1")),
                                          Q_ARG(quint16, server.serverPort()),
                                          Q_ARG(quint16, 0),
                                          Q_ARG(quint64, 99)));

        QTRY_VERIFY_WITH_TIMEOUT(server.hasPendingConnections(), 2000);
        std::unique_ptr<QTcpSocket> peer(server.nextPendingConnection());
        QVERIFY(peer != nullptr);

        // Read and discard handshake first.
        QTRY_VERIFY_WITH_TIMEOUT(peer->bytesAvailable() >= 12 || peer->waitForReadyRead(50), 2000);
        const QByteArray handshake = peer->readAll();
        QVERIFY(handshake.size() >= 12);

        const quint16 clientUdpPort = peer->peerPort();
        QUdpSocket sender;

        const auto firstDelta = makeUdpDatagram(
            1, multilife::client::proto::kFlagDelta, 0, 0, {{2, 3, 1001}});
        QCOMPARE(sender.writeDatagram(firstDelta, QHostAddress::LocalHost, clientUdpPort),
                 static_cast<qint64>(firstDelta.size()));
        QTRY_COMPARE_WITH_TIMEOUT(worldSpy.count(), 1, 2000);

        const auto droppedDelta = makeUdpDatagram(
            3, multilife::client::proto::kFlagDelta, 0, 0, {{4, 5, 1002}});
        QCOMPARE(sender.writeDatagram(droppedDelta, QHostAddress::LocalHost, clientUdpPort),
                 static_cast<qint64>(droppedDelta.size()));
        QTRY_COMPARE_WITH_TIMEOUT(worldSpy.count(), 2, 2000);

        QTRY_VERIFY_WITH_TIMEOUT(peer->bytesAvailable() > 0 || peer->waitForReadyRead(50), 2000);
        const QByteArray afterGap = peer->readAll();
        QVERIFY(afterGap.contains(static_cast<char>(multilife::client::proto::kMsgResyncReq)));

        const auto fullSnapshot = makeUdpDatagram(
            100, multilife::client::proto::kFlagFull, 0, 0, {{8, 9, 2001}});
        QCOMPARE(sender.writeDatagram(fullSnapshot, QHostAddress::LocalHost, clientUdpPort),
                 static_cast<qint64>(fullSnapshot.size()));
        QTRY_COMPARE_WITH_TIMEOUT(worldSpy.count(), 3, 2000);

        const auto trailingDelta = makeUdpDatagram(
            101, multilife::client::proto::kFlagDelta, 0, 0, {{9, 10, 2002}});
        QCOMPARE(sender.writeDatagram(trailingDelta, QHostAddress::LocalHost, clientUdpPort),
                 static_cast<qint64>(trailingDelta.size()));
        QTRY_COMPARE_WITH_TIMEOUT(worldSpy.count(), 4, 2000);

        QCOMPARE(worldSpy[2][1].toBool(), true);
        const auto updates = worldSpy[2][4].value<QVector<AliveCell>>();
        QCOMPARE(updates.size(), 1);
        QCOMPARE(updates[0].x, static_cast<std::int64_t>(8));
        QCOMPARE(updates[0].y, static_cast<std::int64_t>(9));
        QCOMPARE(updates[0].owner, static_cast<std::uint64_t>(2001));

        QCOMPARE(worldSpy[3][1].toBool(), false);
        const auto trailingUpdates = worldSpy[3][4].value<QVector<AliveCell>>();
        QCOMPARE(trailingUpdates.size(), 1);
        QCOMPARE(trailingUpdates[0].x, static_cast<std::int64_t>(9));
        QCOMPARE(trailingUpdates[0].y, static_cast<std::int64_t>(10));
        QCOMPARE(trailingUpdates[0].owner, static_cast<std::uint64_t>(2002));

        QVERIFY(networkThread.isRunning());
    }
};

QTEST_MAIN(NetworkThreadingTest)

#include "test_network_threading_qt.moc"
