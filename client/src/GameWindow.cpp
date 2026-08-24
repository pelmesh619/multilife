#include "GameWindow.h"

#include "InfiniteGridView.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QRandomGenerator>
#include <QSettings>
#include <QSpinBox>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cmath>

namespace {

constexpr std::uint64_t kClientPlaceCost = 2;   // must match server `kPlaceCost`
constexpr std::uint64_t kClientRemoveAward = 1; // must match server `kRemoveAward`

QString actionName(GameWindow::EditAction action)
{
    switch (action) {
    case GameWindow::EditAction::Place:
        return "place";
    case GameWindow::EditAction::Remove:
        return "remove";
    case GameWindow::EditAction::Toggle:
    default:
        return "toggle";
    }
}

std::int64_t floorDiv(std::int64_t value, std::int64_t divisor)
{
    if (divisor <= 0) {
        return 0;
    }
    if (value >= 0) {
        return value / divisor;
    }
    return (value - divisor + 1) / divisor;
}

} // namespace

namespace {

GameWindow::ConnectionParams loadConnectionSettings()
{
    QSettings settings("multilife", "client");
    GameWindow::ConnectionParams params;
    params.playerId = QRandomGenerator::global()->generate64();
    params.host = settings.value("network/host", params.host).toString();
    params.tcpPort = settings.value("network/tcpPort", params.tcpPort).toUInt();
    params.udpPort = settings.value("network/udpPort", params.udpPort).toUInt();
    params.playerId = settings.value("network/playerId", QString::number(params.playerId))
                          .toULongLong();
    return params;
}

void saveConnectionSettings(const GameWindow::ConnectionParams& params)
{
    QSettings settings("multilife", "client");
    settings.setValue("network/host", params.host);
    settings.setValue("network/tcpPort", params.tcpPort);
    settings.setValue("network/udpPort", params.udpPort);
    settings.setValue("network/playerId", QString::number(params.playerId));
}

class ConnectionDialog final : public QDialog
{
public:
    explicit ConnectionDialog(GameWindow::ConnectionParams defaults, QWidget* parent = nullptr)
        : QDialog(parent)
        , m_hostEdit(new QLineEdit(defaults.host, this))
        , m_tcpPortSpin(new QSpinBox(this))
        , m_udpPortSpin(new QSpinBox(this))
        , m_playerIdEdit(new QLineEdit(QString::number(defaults.playerId), this))
    {
        setWindowTitle("Connect to Multilife Server");
        setModal(true);

        m_tcpPortSpin->setRange(1, 65535);
        m_tcpPortSpin->setValue(defaults.tcpPort);

        m_udpPortSpin->setRange(1, 65535);
        m_udpPortSpin->setValue(defaults.udpPort);

        auto* form = new QFormLayout(this);
        form->addRow("Host", m_hostEdit);
        form->addRow("TCP port", m_tcpPortSpin);
        form->addRow("UDP port", m_udpPortSpin);
        form->addRow("Player ID", m_playerIdEdit);

        auto* buttons =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        form->addRow(buttons);

        connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
            bool ok = false;
            const auto value = m_playerIdEdit->text().toULongLong(&ok);
            if (!ok || value == 0) {
                m_playerIdEdit->setText(
                    QString::number(QRandomGenerator::global()->generate64()));
                return;
            }
            m_params.host = m_hostEdit->text().trimmed();
            if (m_params.host.isEmpty()) {
                m_params.host = "127.0.0.1";
            }
            m_params.tcpPort = static_cast<quint16>(m_tcpPortSpin->value());
            m_params.udpPort = static_cast<quint16>(m_udpPortSpin->value());
            m_params.playerId = value;
            accept();
        });
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    }

    GameWindow::ConnectionParams params() const { return m_params; }

private:
    GameWindow::ConnectionParams m_params;
    QLineEdit* m_hostEdit;
    QSpinBox* m_tcpPortSpin;
    QSpinBox* m_udpPortSpin;
    QLineEdit* m_playerIdEdit;
};

} // namespace

GameWindow::GameWindow(bool openConnectDialogOnStart)
    : m_worldModel()
    , m_clientRuntime(this)
    , m_gridView(new InfiniteGridView(this))
    , m_statusLabel(new QLabel("Disconnected", this))
    , m_generationLabel(new QLabel("Generation: 0", this))
    , m_liveCellsLabel(new QLabel("Live cells: 0", this))
    , m_tickDurationLabel(new QLabel("Tick: -", this))
    , m_playerIdLabel(new QLabel("Player ID: -", this))
    , m_balanceLabel(new QLabel("Balance: 0", this))
    , m_myCellsLabel(new QLabel("My cells: 0", this))
    , m_incomeLabel(new QLabel("Income/tick: 0", this))
    , m_centerLabel(new QLabel("Center: (0, 0)", this))
    , m_chunkLabel(new QLabel("Chunk: (0, 0)", this))
    , m_pendingLabel(new QLabel("Pending edits: 0", this))
    , m_disconnectButton(new QPushButton("Disconnect", this))
    , m_resyncButton(new QPushButton("Resync", this))
    , m_actionCombo(new QComboBox(this))
    , m_modeCombo(new QComboBox(this))
    , m_playersList(new QListWidget(this))
    , m_pendingList(new QListWidget(this))
    , m_applyPendingButton(new QPushButton("Apply", this))
    , m_clearPendingButton(new QPushButton("Clear", this))
{
    setWindowTitle("Multilife Qt Client");
    resize(1320, 840);
    m_gridView->setObjectName("gridView");
    m_pendingLabel->setObjectName("pendingLabel");
    m_pendingList->setObjectName("pendingList");
    m_applyPendingButton->setObjectName("applyPendingButton");
    m_clearPendingButton->setObjectName("clearPendingButton");
    m_actionCombo->setObjectName("actionCombo");
    m_modeCombo->setObjectName("modeCombo");

    connect(&m_clientRuntime,
            &ClientRuntime::worldUpdateReceived,
            this,
            [this](std::uint32_t seqNum,
                   bool fullSnapshot,
                   std::int32_t chunkX,
                   std::int32_t chunkY,
                   const QVector<AliveCell>& cells) {
                m_worldModel.applyUpdate(seqNum, fullSnapshot, chunkX, chunkY, cells);
            },
            Qt::QueuedConnection);

    m_gridView->setWorldModel(&m_worldModel);
    setCentralWidget(m_gridView);
    addDockWidget(Qt::RightDockWidgetArea, createSidePanel());

    auto* toolbar = addToolBar("Network");
    toolbar->setMovable(false);
    auto* connectButton = new QPushButton("Connect", this);
    toolbar->addWidget(connectButton);
    toolbar->addWidget(m_disconnectButton);
    toolbar->addWidget(m_resyncButton);

    statusBar()->addWidget(m_statusLabel, 1);

    m_actionCombo->addItem("Toggle", static_cast<int>(EditAction::Toggle));
    m_actionCombo->addItem("Place", static_cast<int>(EditAction::Place));
    m_actionCombo->addItem("Remove", static_cast<int>(EditAction::Remove));
    m_modeCombo->addItem("Immediate", static_cast<int>(EditMode::Immediate));
    m_modeCombo->addItem("Queue then Apply", static_cast<int>(EditMode::Queued));

    connect(&m_worldModel,
            &WorldModel::statsChanged,
            this,
            [this](std::uint32_t generation, std::size_t liveCells) {
                m_generationLabel->setText(QString("Generation: %1").arg(generation));
                m_liveCellsLabel->setText(QString("Live cells: %1").arg(liveCells));
                if (m_tickTimerValid) {
                    const qint64 ms = m_tickTimer.restart();
                    if (ms > 0) {
                        m_tickDurationLabel->setText(QString("Tick: %1 ms").arg(ms));
                    }
                } else {
                    m_tickTimer.start();
                    m_tickTimerValid = true;
                }
                refreshStatsPanel();
            });
    connect(&m_clientRuntime,
            &ClientRuntime::connectionStateChanged,
            this,
            [this](const QString& text, bool connected) {
                m_statusLabel->setText(text);
                if (connected) {
                    saveConnectionSettings(m_lastParams);
                } else {
                    m_serverPlayers.clear();
                    refreshStatsPanel();
                }
            },
            Qt::QueuedConnection);
    connect(&m_clientRuntime,
            &ClientRuntime::serverStatsUpdated,
            this,
            [this](std::uint32_t, const QVector<GameNetworkClient::PlayerSnapshot>& players) {
                onServerStatsUpdated(players);
            },
            Qt::QueuedConnection);
    connect(m_gridView,
            &InfiniteGridView::cellClicked,
            this,
            [this](std::int64_t x, std::int64_t y) { onGridCellClicked(x, y); });
    connect(m_gridView, &InfiniteGridView::viewRectChanged, this, [this](const QRectF& rect) {
        updateNavigationPanel(rect);
    });
    connect(connectButton, &QPushButton::clicked, this, [this]() { openConnectionDialog(); });
    connect(m_disconnectButton,
            &QPushButton::clicked,
            this,
            [this]() { m_clientRuntime.disconnectFromServer(); });
    connect(m_resyncButton,
            &QPushButton::clicked,
            this,
            [this]() { m_clientRuntime.sendResync(); });
    connect(m_applyPendingButton,
            &QPushButton::clicked,
            this,
            [this]() { applyPendingEdits(); });
    connect(m_clearPendingButton, &QPushButton::clicked, this, [this]() {
        m_pendingEdits.clear();
        refreshPendingList();
        m_gridView->setPendingEdits({});
    });

    refreshPendingList();
    refreshStatsPanel();
    updateNavigationPanel(m_gridView->mapToScene(m_gridView->viewport()->rect()).boundingRect());
    if (openConnectDialogOnStart) {
        openConnectionDialog();
    }
}

QDockWidget* GameWindow::createSidePanel()
{
    auto* dock = new QDockWidget("Game Panel", this);
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

    auto* panel = new QWidget(dock);
    auto* layout = new QVBoxLayout(panel);

    auto* profileGroup = new QGroupBox("Profile", panel);
    auto* profileLayout = new QVBoxLayout(profileGroup);
    profileLayout->addWidget(m_playerIdLabel);
    profileLayout->addWidget(m_balanceLabel);
    profileLayout->addWidget(m_myCellsLabel);
    profileLayout->addWidget(m_incomeLabel);
    layout->addWidget(profileGroup);

    auto* sessionGroup = new QGroupBox("Session", panel);
    auto* sessionLayout = new QVBoxLayout(sessionGroup);
    sessionLayout->addWidget(m_generationLabel);
    sessionLayout->addWidget(m_liveCellsLabel);
    sessionLayout->addWidget(m_tickDurationLabel);
    sessionLayout->addWidget(m_pendingLabel);
    layout->addWidget(sessionGroup);

    auto* editGroup = new QGroupBox("Edits", panel);
    auto* editLayout = new QFormLayout(editGroup);
    editLayout->addRow("Action", m_actionCombo);
    editLayout->addRow("Mode", m_modeCombo);
    layout->addWidget(editGroup);

    auto* pendingGroup = new QGroupBox("Pending", panel);
    auto* pendingLayout = new QVBoxLayout(pendingGroup);
    pendingLayout->addWidget(m_pendingList);
    auto* buttons = new QHBoxLayout();
    buttons->addWidget(m_applyPendingButton);
    buttons->addWidget(m_clearPendingButton);
    pendingLayout->addLayout(buttons);
    layout->addWidget(pendingGroup);

    auto* playersGroup = new QGroupBox("Players", panel);
    auto* playersLayout = new QVBoxLayout(playersGroup);
    playersLayout->addWidget(m_playersList);
    layout->addWidget(playersGroup);

    auto* navGroup = new QGroupBox("Navigation", panel);
    auto* navLayout = new QVBoxLayout(navGroup);
    navLayout->addWidget(m_centerLabel);
    navLayout->addWidget(m_chunkLabel);
    layout->addWidget(navGroup);

    layout->addStretch(1);
    dock->setWidget(panel);
    return dock;
}

GameWindow::EditAction GameWindow::currentAction() const
{
    return static_cast<EditAction>(m_actionCombo->currentData().toInt());
}

GameWindow::EditMode GameWindow::currentMode() const
{
    return static_cast<EditMode>(m_modeCombo->currentData().toInt());
}

void GameWindow::sendAction(EditAction action, std::int64_t x, std::int64_t y)
{
    const auto meIt = m_serverPlayers.find(m_lastParams.playerId);
    const bool balanceKnown = meIt != m_serverPlayers.end();
    const std::uint64_t balance = balanceKnown ? meIt->balance : 0ULL;
    const std::uint64_t owner = m_worldModel.ownerAt(x, y);

    auto reject = [this](const QString& reason) {
        m_statusLabel->setText(reason);
    };

    switch (action) {
    case EditAction::Place:
        if (owner != 0) {
            reject("Can't place: cell is already owned");
            return;
        }
        if (balanceKnown && balance < kClientPlaceCost) {
            reject(QString("Not enough balance to place (need %1)").arg(kClientPlaceCost));
            return;
        }
        m_clientRuntime.sendPlaceCell(x, y);
        break;
    case EditAction::Remove:
        if (owner != m_lastParams.playerId) {
            reject("Can't remove: not your cell");
            return;
        }
        m_clientRuntime.sendRemoveCell(x, y);
        break;
    case EditAction::Toggle:
    default:
        if (owner != 0 && owner != m_lastParams.playerId) {
            reject("Can't toggle: not your cell");
            return;
        }
        if (balanceKnown && owner == 0 && balance < kClientPlaceCost) {
            reject(QString("Not enough balance to toggle on (need %1)").arg(kClientPlaceCost));
            return;
        }
        m_clientRuntime.sendToggleCell(x, y);
        break;
    }
}

void GameWindow::onGridCellClicked(std::int64_t x, std::int64_t y)
{
    const auto action = currentAction();
    if (currentMode() == EditMode::Immediate) {
        sendAction(action, x, y);
        return;
    }

    const auto meIt = m_serverPlayers.find(m_lastParams.playerId);
    const bool balanceKnown = meIt != m_serverPlayers.end();
    const std::uint64_t balance = balanceKnown ? meIt->balance : 0ULL;
    const std::uint64_t owner = m_worldModel.ownerAt(x, y);

    auto reject = [this](const QString& reason) {
        m_statusLabel->setText(reason);
    };

    auto willSpend = [&](EditAction a, std::uint64_t cellOwner) -> std::uint64_t {
        switch (a) {
        case EditAction::Place:
            return (cellOwner == 0) ? kClientPlaceCost : 0;
        case EditAction::Toggle:
            return (cellOwner == 0) ? kClientPlaceCost : 0;
        case EditAction::Remove:
        default:
            return 0;
        }
    };

    auto isAllowed = [&](EditAction a, std::uint64_t cellOwner) -> bool {
        switch (a) {
        case EditAction::Place:
            return cellOwner == 0;
        case EditAction::Remove:
            return cellOwner == m_lastParams.playerId;
        case EditAction::Toggle:
        default:
            return cellOwner == 0 || cellOwner == m_lastParams.playerId;
        }
    };

    if (!isAllowed(action, owner)) {
        reject("Operation not permitted for this cell");
        return;
    }

    // balance check for queued spending operations
    if (balanceKnown) {
        std::uint64_t queuedSpend = 0;
        for (auto it = m_pendingEdits.constBegin(); it != m_pendingEdits.constEnd(); ++it) {
            const std::uint64_t o = m_worldModel.ownerAt(it.key().x, it.key().y);
            queuedSpend += willSpend(it.value(), o);
        }
        const std::uint64_t extra = willSpend(action, owner);
        if (queuedSpend + extra > balance) {
            reject(QString("Not enough balance for queued ops (%1/%2)")
                       .arg(queuedSpend + extra)
                       .arg(balance));
            return;
        }
    }

    const CellKey key{x, y};
    const auto it = m_pendingEdits.find(key);
    if (it != m_pendingEdits.end() && it.value() == action) {
        m_pendingEdits.erase(it);
    } else {
        m_pendingEdits.insert(key, action);
    }
    refreshPendingList();
    QHash<CellKey, int> overlay;
    overlay.reserve(m_pendingEdits.size());
    for (auto it2 = m_pendingEdits.constBegin(); it2 != m_pendingEdits.constEnd(); ++it2) {
        overlay.insert(it2.key(), static_cast<int>(it2.value()));
    }
    m_gridView->setPendingEdits(overlay);
}

void GameWindow::applyPendingEdits()
{
    if (m_pendingEdits.isEmpty()) {
        return;
    }
    for (auto it = m_pendingEdits.constBegin(); it != m_pendingEdits.constEnd(); ++it) {
        sendAction(it.value(), it.key().x, it.key().y);
    }
    m_pendingEdits.clear();
    refreshPendingList();
    m_gridView->setPendingEdits({});
}

void GameWindow::refreshPendingList()
{
    m_pendingList->clear();
    QVector<CellKey> keys;
    keys.reserve(m_pendingEdits.size());
    for (auto it = m_pendingEdits.constBegin(); it != m_pendingEdits.constEnd(); ++it) {
        keys.push_back(it.key());
    }
    std::sort(keys.begin(), keys.end(), [](const CellKey& lhs, const CellKey& rhs) {
        if (lhs.y == rhs.y) {
            return lhs.x < rhs.x;
        }
        return lhs.y < rhs.y;
    });
    for (const auto& key : keys) {
        const auto action = m_pendingEdits.value(key);
        m_pendingList->addItem(QString("%1 (%2, %3)").arg(actionName(action)).arg(key.x).arg(key.y));
    }
    m_pendingLabel->setText(QString("Pending edits: %1").arg(m_pendingEdits.size()));
}

void GameWindow::refreshStatsPanel()
{
    m_playerIdLabel->setText(QString("Player ID: %1").arg(m_lastParams.playerId));

    m_playersList->clear();
    if (m_serverPlayers.isEmpty()) {
        const auto myCells = m_worldModel.liveCellsOwnedBy(m_lastParams.playerId);
        m_balanceLabel->setText("Balance: -");
        m_myCellsLabel->setText(QString("My cells: %1").arg(myCells));
        m_incomeLabel->setText(QString("Income/tick: %1").arg(myCells));

        const auto stats = m_worldModel.ownerStats();
        for (const auto& [playerId, cells] : stats) {
            m_playersList->addItem(
                QString("Player %1 | cells: %2 | balance: ?").arg(playerId).arg(cells));
        }
        return;
    }

    const auto me = m_serverPlayers.find(m_lastParams.playerId);
    if (me == m_serverPlayers.end()) {
        m_balanceLabel->setText("Balance: 0");
        m_myCellsLabel->setText("My cells: 0");
        m_incomeLabel->setText("Income/tick: 0");
    } else {
        m_balanceLabel->setText(QString("Balance: %1").arg(me->balance));
        m_myCellsLabel->setText(QString("My cells: %1").arg(me->liveCells));
        m_incomeLabel->setText(QString("Income/tick: %1").arg(me->liveCells));
    }

    QVector<std::uint64_t> playerIds;
    playerIds.reserve(m_serverPlayers.size());
    for (auto it = m_serverPlayers.constBegin(); it != m_serverPlayers.constEnd(); ++it) {
        playerIds.push_back(it.key());
    }
    std::sort(playerIds.begin(), playerIds.end(), [this](std::uint64_t lhs, std::uint64_t rhs) {
        const auto l = m_serverPlayers.value(lhs);
        const auto r = m_serverPlayers.value(rhs);
        if (l.liveCells == r.liveCells) {
            return lhs < rhs;
        }
        return l.liveCells > r.liveCells;
    });
    for (const auto playerId : playerIds) {
        const auto& stats = m_serverPlayers[playerId];
        m_playersList->addItem(QString("Player %1 | cells: %2 | balance: %3")
                                   .arg(playerId)
                                   .arg(stats.liveCells)
                                   .arg(stats.balance));
    }
}

void GameWindow::onServerStatsUpdated(const QVector<GameNetworkClient::PlayerSnapshot>& players)
{
    m_serverPlayers.clear();
    for (const auto& p : players) {
        m_serverPlayers.insert(p.playerId, p);
    }
    refreshStatsPanel();
}

void GameWindow::updateNavigationPanel(const QRectF& viewRect)
{
    const auto center = viewRect.center();
    const auto cx = static_cast<std::int64_t>(std::llround(center.x()));
    const auto cy = static_cast<std::int64_t>(std::llround(center.y()));
    m_centerLabel->setText(QString("Center: (%1, %2)").arg(cx).arg(cy));

    const auto chunkX = floorDiv(cx, 64);
    const auto chunkY = floorDiv(cy, 64);
    m_chunkLabel->setText(QString("Chunk: (%1, %2)").arg(chunkX).arg(chunkY));
}

void GameWindow::openConnectionDialog()
{
    const auto defaults = loadConnectionSettings();
    ConnectionDialog dialog(defaults, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    m_lastParams = dialog.params();
    refreshStatsPanel();
    m_worldModel.reset();
    m_tickDurationLabel->setText("Tick: -");
    m_tickTimerValid = false;
    m_clientRuntime.connectToServer(
        m_lastParams.host, m_lastParams.tcpPort, m_lastParams.udpPort, m_lastParams.playerId);
}
