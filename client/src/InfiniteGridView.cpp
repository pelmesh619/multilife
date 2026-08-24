#include "InfiniteGridView.h"

#include <QPainter>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QWidget>
#include <QtMath>

class MiniMapOverlay final : public QWidget
{
public:
    explicit MiniMapOverlay(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
        setFixedSize(220, 220);
    }

    void setWorldModel(WorldModel* model)
    {
        if (m_worldModel == model) {
            return;
        }
        if (m_worldModel) {
            disconnect(m_worldModel, nullptr, this, nullptr);
        }
        m_worldModel = model;
        if (m_worldModel) {
            connect(m_worldModel, &WorldModel::worldChanged, this, [this]() { update(); });
        }
        update();
    }

    void setViewRect(const QRectF& rect)
    {
        m_viewRect = rect;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 255, 255, 220));
        painter.drawRoundedRect(this->rect().adjusted(0, 0, -1, -1), 10.0, 10.0);

        painter.setPen(QPen(QColor(180, 184, 190, 220), 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(this->rect().adjusted(0, 0, -1, -1), 10.0, 10.0);

        if (!m_worldModel || m_viewRect.isEmpty()) {
            return;
        }

        const QPointF center = m_viewRect.center();
        constexpr qreal halfSpan = 256.0;
        const QRectF worldRect(center.x() - halfSpan,
                               center.y() - halfSpan,
                               halfSpan * 2.0,
                               halfSpan * 2.0);

        auto toMini = [this, &worldRect](const QPointF& worldPoint) -> QPointF {
            const qreal nx = (worldPoint.x() - worldRect.left()) / worldRect.width();
            const qreal ny = (worldPoint.y() - worldRect.top()) / worldRect.height();
            return QPointF(nx * width(), ny * height());
        };

        const auto cells = m_worldModel->visibleCells(worldRect);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(70, 128, 230, 200));
        for (const auto& c : cells) {
            const auto p = toMini(QPointF(static_cast<qreal>(c.x), static_cast<qreal>(c.y)));
            painter.drawEllipse(p, 1.2, 1.2);
        }

        const QPointF topLeft = toMini(m_viewRect.topLeft());
        const QPointF bottomRight = toMini(m_viewRect.bottomRight());
        QRectF vp(topLeft, bottomRight);
        vp = vp.normalized();
        vp = vp.intersected(QRectF(0, 0, width(), height()));
        painter.setPen(QPen(QColor(214, 53, 53, 220), 1.5));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(vp);
    }

private:
    WorldModel* m_worldModel{nullptr};
    QRectF m_viewRect;
};

InfiniteGridView::InfiniteGridView(QWidget *parent)
    : QGraphicsView(parent)
{
    auto scene = new QGraphicsScene(this);
    scene->setSceneRect(-10000, -10000, 20000, 20000);
    setScene(scene);

    setRenderHint(QPainter::Antialiasing, false);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);

    m_miniMap = new MiniMapOverlay(this);
    m_miniMap->move(10, 10);

    m_animTimer.start();

    m_animTick = new QTimer(this);

    connect(m_animTick, &QTimer::timeout, this, [this]() {

        constexpr float speed = 0.12f;

        bool changed = false;

        for (auto it = m_cellVisuals.begin(); it != m_cellVisuals.end();) {

            auto& v = it.value();

            if (v.targetAlive) {
                v.life = qMin(1.0f, v.life + speed);
            } else {
                v.life = qMax(0.0f, v.life - speed);
            }

            if (!v.targetAlive && v.life <= 0.001f) {
                it = m_cellVisuals.erase(it);
                changed = true;
                continue;
            }

            ++it;
            changed = true;
        }

        if (changed) {
            viewport()->update();
        }
    });

    m_animTick->start(16);
}

void InfiniteGridView::setWorldModel(WorldModel* worldModel)
{
    if (m_worldModel == worldModel) {
        return;
    }
    if (m_worldModel) {
        disconnect(m_worldModel, nullptr, this, nullptr);
    }
    m_worldModel = worldModel;
    if (m_worldModel) {
        connect(m_worldModel, &WorldModel::worldChanged, this, [this]() {
            syncVisualState();
            viewport()->update();
        });
    }
    if (m_miniMap) {
        m_miniMap->setWorldModel(m_worldModel);
    }
    syncVisualState();
    viewport()->update();
    emitViewRectChanged();
}

void InfiniteGridView::setPendingEdits(const QHash<CellKey, int>& pendingEdits)
{
    m_pendingEdits = pendingEdits;
    syncVisualState();
    viewport()->update();
}

void InfiniteGridView::wheelEvent(QWheelEvent *event)
{
    const qreal minScale = 0.01;
    const qreal minVisibleCellsInWidth = 10.0;
    const qreal viewportWidth = qMax(1, viewport()->width());
    const qreal maxScale = qMax(5.0, viewportWidth / minVisibleCellsInWidth);

    qreal currentScale = transform().m11();
    qreal scaleFactor = 1.1;

    if (event->angleDelta().y() > 0) {
        if (currentScale >= maxScale) {
            event->accept();
            return;
        }
        const qreal newScale = qMin(currentScale * scaleFactor, maxScale);
        const qreal factor = newScale / currentScale;
        scale(factor, factor);
    } else {
        if (currentScale <= minScale) {
            event->accept();
            return;
        }
        const qreal newScale = qMax(currentScale / scaleFactor, minScale);
        const qreal factor = newScale / currentScale;
        scale(factor, factor);
    }
    event->accept();
    emitViewRectChanged();
}
void InfiniteGridView::drawBackground(QPainter *painter, const QRectF &rect)
{
    QGraphicsView::drawBackground(painter, rect);

    qreal scaleX = transform().m11();
    qreal scaleY = transform().m22();
    qreal scale = qMin(qAbs(scaleX), qAbs(scaleY));

    if (scale < 0.0001) scale = 0.0001;

    const bool cellLevelGrid = scale >= 12.0;
    const qreal right = rect.right();
    const qreal bottom = rect.bottom();

    if (cellLevelGrid) {
        const qreal cellStep = 1.0;
        const qreal left = qFloor(rect.left() / cellStep) * cellStep;
        const qreal top = qFloor(rect.top() / cellStep) * cellStep;

        // minor cell grid
        painter->setPen(QPen(QColor(232, 232, 232), 0));
        for (qreal x = left; x <= right; x += cellStep) {
            painter->drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
        }
        for (qreal y = top; y <= bottom; y += cellStep) {
            painter->drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));
        }

        // major chunk boundaries every 64 cells
        constexpr qreal chunkStep = 64.0;
        const qreal chunkLeft = qFloor(rect.left() / chunkStep) * chunkStep;
        const qreal chunkTop = qFloor(rect.top() / chunkStep) * chunkStep;
        painter->setPen(QPen(QColor(150, 150, 150), 0));
        for (qreal x = chunkLeft; x <= right; x += chunkStep) {
            painter->drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
        }
        for (qreal y = chunkTop; y <= bottom; y += chunkStep) {
            painter->drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));
        }

        
        painter->save();

        QTransform originalTransform = painter->transform();
        painter->resetTransform();

        QFont font = painter->font();
        font.setPointSize(8);

        painter->setFont(font);
        painter->setPen(QColor(90, 90, 90, 160));

        constexpr qreal labelPadding = 3.0;

        for (qreal x = chunkLeft; x <= right; x += chunkStep) {
            for (qreal y = chunkTop; y <= bottom; y += chunkStep) {

                const int chunkX = static_cast<int>(std::floor(x / chunkStep));
                const int chunkY = static_cast<int>(std::floor(y / chunkStep));

                QString text = QString("%1:%2")
                                .arg(chunkX)
                                .arg(chunkY);

                QPointF scenePos(x + labelPadding,
                                y + labelPadding);

                QPoint viewPos = mapFromScene(scenePos);

                painter->drawText(viewPos, text);
            }
        }

        painter->setTransform(originalTransform);
        painter->restore();
    } else {
        const qreal targetPixelSize = 80.0;
        qreal sceneInterval = targetPixelSize / scale;
        qreal gridStep = getSuitableGridInterval(sceneInterval);

        qreal estimatedHorizontalLines = (right - rect.left()) / gridStep;
        qreal estimatedVerticalLines = (bottom - rect.top()) / gridStep;
        const int maxLines = 5000;

        if (estimatedHorizontalLines > maxLines || estimatedVerticalLines > maxLines) {
            qreal factor = qMax(estimatedHorizontalLines / maxLines,
                                estimatedVerticalLines / maxLines);
            gridStep *= factor;
        }

        painter->setPen(QPen(QColor(215, 215, 215), 0));

        const qreal left = qFloor(rect.left() / gridStep) * gridStep;
        const qreal top = qFloor(rect.top() / gridStep) * gridStep;

        for (qreal x = left; x <= right; x += gridStep) {
            painter->drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
        }
        for (qreal y = top; y <= bottom; y += gridStep) {
            painter->drawLine(QPointF(rect.left(), y), QPointF(right, y));
        }
    }

    QPen axisPen(QColor(211, 70, 70), 0);
    painter->setPen(axisPen);
    painter->drawLine(QPointF(0, rect.top()), QPointF(0, rect.bottom()));
    painter->drawLine(QPointF(rect.left(), 0), QPointF(rect.right(), 0));

}

void InfiniteGridView::drawForeground(QPainter* painter, const QRectF& rect)
{
    QGraphicsView::drawForeground(painter, rect);

    if (!m_worldModel) {
        return;
    }

    const auto visibleCells = m_worldModel->visibleCells(rect);
    painter->setPen(Qt::NoPen);

    qreal scale = qMin(qAbs(transform().m11()), qAbs(transform().m22()));
    if (scale < 0.0001) {
        scale = 0.0001;
    }

    // keep cells visible even when zoomed out heavily
    const qreal minScreenPx = 2.0;
    const qreal minSceneSize = minScreenPx / scale;
    const qreal drawSize = qMax<qreal>(1.0, minSceneSize);
    const qreal offset = (drawSize - 1.0) * 0.5;

    for (const auto& cell : visibleCells) {
        // painter->setBrush(ownerColor(cell.owner));
        // painter->drawRect(
        //     QRectF(
        //         static_cast<qreal>(cell.x) - offset,
        //         static_cast<qreal>(cell.y) - offset,
        //         drawSize,
        //         drawSize));

        CellKey key{
            cell.x,
            cell.y
        };

        float t = 1.0f;

        auto it = m_cellVisuals.find(key);
        if (it != m_cellVisuals.end()) {
            t = it.value().life;
        }

        const qreal animatedSize = drawSize * (0.4 + 0.6 * t);

        const qreal animatedOffset =
            (animatedSize - 1.0) * 0.5;

        QColor color = ownerColor(cell.owner);
        color.setAlphaF(t);

        painter->setBrush(color);

        painter->drawRect(
            QRectF(
                static_cast<qreal>(cell.x) - animatedOffset,
                static_cast<qreal>(cell.y) - animatedOffset,
                animatedSize,
                animatedSize
            )
        );
    }

    if (!m_pendingEdits.isEmpty()) {
        for (auto it = m_pendingEdits.constBegin(); it != m_pendingEdits.constEnd(); ++it) {
            const auto& key = it.key();
            const QRectF r(static_cast<qreal>(key.x) - offset,
                           static_cast<qreal>(key.y) - offset,
                           drawSize,
                           drawSize);
            if (!rect.intersects(r)) {
                continue;
            }
            QBrush b(pendingColor(it.value()), Qt::BDiagPattern);
            painter->setBrush(b);
            painter->drawRect(r);
        }
    }
}

qreal InfiniteGridView::getSuitableGridInterval(qreal sceneInterval) const
{
    if (sceneInterval <= 0.0) {
        return 1.0;
    }

    // choose a "nice" step
    const qreal exponent = qFloor(qLn(sceneInterval) / qLn(10.0));
    const qreal base = qPow(10.0, exponent);
    const qreal normalized = sceneInterval / base;

    qreal stepFactor = 1.0;
    if (normalized <= 1.0) {
        stepFactor = 1.0;
    } else if (normalized <= 2.0) {
        stepFactor = 2.0;
    } else if (normalized <= 5.0) {
        stepFactor = 5.0;
    } else {
        stepFactor = 10.0;
    }

    return stepFactor * base;
}

QColor InfiniteGridView::ownerColor(std::uint64_t ownerId) const
{
    const int hue = static_cast<int>((ownerId * 2654435761ULL) % 360ULL);
    return QColor::fromHsv(hue, 220, 230);
}

QColor InfiniteGridView::pendingColor(int action) const
{
    switch (action) {
    case 1: // toggle
        return QColor(70, 128, 230, 170);
    case 2: // place
        return QColor(214, 53, 53, 170);
    case 0: // remove
    default:
        return QColor(120, 120, 120, 170);
    }
}

void InfiniteGridView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_leftPressed = true;
        m_dragDetected = false;
        m_leftPressPos = event->pos();
    }
    QGraphicsView::mousePressEvent(event);
}

void InfiniteGridView::mouseMoveEvent(QMouseEvent* event)
{
    if (m_leftPressed) {
        const int distance = (event->pos() - m_leftPressPos).manhattanLength();
        if (distance > 4) {
            m_dragDetected = true;
        }
    }
    QGraphicsView::mouseMoveEvent(event);
}

void InfiniteGridView::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_leftPressed && !m_dragDetected) {
        const QPointF scenePoint = mapToScene(event->pos());
        emit cellClicked(
            static_cast<std::int64_t>(qFloor(scenePoint.x())),
            static_cast<std::int64_t>(qFloor(scenePoint.y())));
    }
    if (event->button() == Qt::LeftButton) {
        m_leftPressed = false;
        m_dragDetected = false;
    }
    QGraphicsView::mouseReleaseEvent(event);
    emitViewRectChanged();
}

void InfiniteGridView::resizeEvent(QResizeEvent* event)
{
    QGraphicsView::resizeEvent(event);
    if (!m_initialScaleApplied && viewport() && viewport()->width() > 0) {
        // default is ~30 cells visible
        constexpr qreal targetCellsInWidth = 30.0;
        const qreal viewportWidth = qMax(1, viewport()->width());
        const qreal targetScale = qMax<qreal>(0.01, viewportWidth / targetCellsInWidth);
        resetTransform();
        scale(targetScale, targetScale);
        centerOn(0.0, 0.0);
        m_initialScaleApplied = true;
    }
    if (m_miniMap) {
        m_miniMap->move(10, 10);
    }
    emitViewRectChanged();
}

void InfiniteGridView::scrollContentsBy(int dx, int dy)
{
    QGraphicsView::scrollContentsBy(dx, dy);
    emitViewRectChanged();
}

void InfiniteGridView::emitViewRectChanged()
{
    m_lastViewRect = mapToScene(viewport()->rect()).boundingRect();
    if (m_miniMap) {
        m_miniMap->setViewRect(m_lastViewRect);
    }
    emit viewRectChanged(m_lastViewRect);
}

void InfiniteGridView::syncVisualState()
{
    if (!m_worldModel) {
        return;
    }

    const QRectF hugeRect(-1e9, -1e9, 2e9, 2e9);

    const auto cells = m_worldModel->visibleCells(hugeRect);

    QSet<CellKey> aliveNow;

    for (const auto& c : cells) {

        CellKey key{
            static_cast<std::int64_t>(c.x),
            static_cast<std::int64_t>(c.y)
        };

        aliveNow.insert(key);

        auto& visual = m_cellVisuals[key];

        visual.targetAlive = true;

        if (visual.life <= 0.0f) {
            visual.life = 0.0f;
        }
    }

    for (auto it = m_cellVisuals.begin(); it != m_cellVisuals.end(); ++it) {

        if (!aliveNow.contains(it.key())) {
            it.value().targetAlive = false;
        }
    }
}

