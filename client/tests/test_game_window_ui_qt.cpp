#include "GameWindow.h"
#include "InfiniteGridView.h"

#include <QComboBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QPoint>
#include <QTest>

#include <cmath>

class GameWindowUiTest final : public QObject
{
    Q_OBJECT

private slots:
    void realClickPathUpdatesPendingWidgets()
    {
        GameWindow window(false);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        auto* modeCombo = window.findChild<QComboBox*>("modeCombo");
        auto* actionCombo = window.findChild<QComboBox*>("actionCombo");
        auto* pendingList = window.findChild<QListWidget*>("pendingList");
        auto* pendingLabel = window.findChild<QLabel*>("pendingLabel");
        auto* applyButton = window.findChild<QPushButton*>("applyPendingButton");
        auto* grid = window.findChild<InfiniteGridView*>("gridView");

        QVERIFY(modeCombo != nullptr);
        QVERIFY(actionCombo != nullptr);
        QVERIFY(pendingList != nullptr);
        QVERIFY(pendingLabel != nullptr);
        QVERIFY(applyButton != nullptr);
        QVERIFY(grid != nullptr);

        modeCombo->setCurrentIndex(1);   // Queue then Apply
        actionCombo->setCurrentIndex(1); // Place

        const QPoint clickPos = grid->viewport()->rect().center();
        const QPointF scenePos = grid->mapToScene(clickPos);
        const auto expectedX = static_cast<std::int64_t>(std::floor(scenePos.x()));
        const auto expectedY = static_cast<std::int64_t>(std::floor(scenePos.y()));
        const QString expectedItem = QString("place (%1, %2)").arg(expectedX).arg(expectedY);

        QTest::mouseClick(grid->viewport(), Qt::LeftButton, Qt::NoModifier, clickPos);

        QCOMPARE(pendingList->count(), 1);
        QCOMPARE(pendingList->item(0)->text(), expectedItem);
        QCOMPARE(pendingLabel->text(), QString("Pending edits: 1"));

        QTest::mouseClick(applyButton, Qt::LeftButton);
        QTRY_COMPARE_WITH_TIMEOUT(pendingList->count(), 0, 1000);
        QCOMPARE(pendingLabel->text(), QString("Pending edits: 0"));
    }
};

QTEST_MAIN(GameWindowUiTest)

#include "test_game_window_ui_qt.moc"
