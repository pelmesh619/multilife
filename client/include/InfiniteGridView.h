#pragma once

#include <QColor>
#include <QElapsedTimer>
#include <QGraphicsView>
#include <QResizeEvent>
#include <QPoint>
#include <QRectF>
#include <QHash>
#include <QTimer>
#include "WorldModel.h"
#include <cstdint>

class WorldModel;
class MiniMapOverlay;

struct CellVisualState {
    float life = 0.0f;
    bool targetAlive = false;
};



class InfiniteGridView : public QGraphicsView
{
    Q_OBJECT

public:
    explicit InfiniteGridView(QWidget *parent = nullptr);
    void setWorldModel(WorldModel* worldModel);
    void setPendingEdits(const QHash<CellKey, int>& pendingEdits);

protected:
    void drawBackground(QPainter *painter, const QRectF &rect) override;
    void drawForeground(QPainter* painter, const QRectF& rect) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void scrollContentsBy(int dx, int dy) override;

signals:
    void cellClicked(std::int64_t x, std::int64_t y);
    void viewRectChanged(const QRectF& viewRect);

private:
    qreal getSuitableGridInterval(qreal sceneInterval) const;
    QColor ownerColor(std::uint64_t ownerId) const;
    QColor pendingColor(int action) const;
    void emitViewRectChanged();

    WorldModel* m_worldModel{nullptr};
    QHash<CellKey, int> m_pendingEdits;
    MiniMapOverlay* m_miniMap{nullptr};
    QRectF m_lastViewRect;
    QPoint m_leftPressPos;
    bool m_leftPressed{false};
    bool m_dragDetected{false};
    bool m_initialScaleApplied{false};

    QHash<CellKey, CellVisualState> m_cellVisuals;
    QElapsedTimer m_animTimer;
    QTimer* m_animTick{nullptr};

    void syncVisualState();
};
