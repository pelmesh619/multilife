#pragma once

#include "ClientRuntime.h"
#include "GameNetworkClient.h"
#include "WorldModel.h"

#include <QHash>
#include <QMainWindow>
#include <QElapsedTimer>

#include <cstdint>

class InfiniteGridView;
class QLabel;
class QPushButton;
class QComboBox;
class QListWidget;
class QDockWidget;

class GameWindow final : public QMainWindow
{
public:
    explicit GameWindow(bool openConnectDialogOnStart = true);
    struct ConnectionParams {
        QString host{"127.0.0.1"};
        quint16 tcpPort{9000};
        quint16 udpPort{9001};
        std::uint64_t playerId{1};
    };

    enum class EditAction {
        Toggle = 0,
        Place = 1,
        Remove = 2
    };

    enum class EditMode {
        Immediate = 0,
        Queued = 1
    };

private:

    QDockWidget* createSidePanel();
    EditAction currentAction() const;
    EditMode currentMode() const;
    void sendAction(EditAction action, std::int64_t x, std::int64_t y);
    void onGridCellClicked(std::int64_t x, std::int64_t y);
    void applyPendingEdits();
    void refreshPendingList();
    void refreshStatsPanel();
    void onServerStatsUpdated(const QVector<GameNetworkClient::PlayerSnapshot>& players);
    void updateNavigationPanel(const QRectF& viewRect);
    void openConnectionDialog();

    WorldModel m_worldModel;
    ClientRuntime m_clientRuntime;
    InfiniteGridView* m_gridView;
    QLabel* m_statusLabel;
    QLabel* m_generationLabel;
    QLabel* m_liveCellsLabel;
    QLabel* m_tickDurationLabel;
    QLabel* m_playerIdLabel;
    QLabel* m_balanceLabel;
    QLabel* m_myCellsLabel;
    QLabel* m_incomeLabel;
    QLabel* m_centerLabel;
    QLabel* m_chunkLabel;
    QLabel* m_pendingLabel;
    QPushButton* m_disconnectButton;
    QPushButton* m_resyncButton;
    QComboBox* m_actionCombo;
    QComboBox* m_modeCombo;
    QListWidget* m_playersList;
    QListWidget* m_pendingList;
    QPushButton* m_applyPendingButton;
    QPushButton* m_clearPendingButton;
    QHash<CellKey, EditAction> m_pendingEdits;
    QHash<std::uint64_t, GameNetworkClient::PlayerSnapshot> m_serverPlayers;
    ConnectionParams m_lastParams;

    QElapsedTimer m_tickTimer;
    bool m_tickTimerValid{false};
};
