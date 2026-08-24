#pragma once

#include <QHash>
#include <QMetaType>
#include <QObject>
#include <QPair>
#include <QRectF>
#include <QVector>

#include <cstdint>

struct CellKey {
    std::int64_t x{0};
    std::int64_t y{0};

    bool operator==(const CellKey& other) const noexcept
    {
        return x == other.x && y == other.y;
    }
};

struct AliveCell {
    std::int64_t x{0};
    std::int64_t y{0};
    std::uint64_t owner{0};
};

Q_DECLARE_METATYPE(AliveCell)

uint qHash(const CellKey& key, uint seed = 0) noexcept;

class WorldModel final : public QObject
{
    Q_OBJECT

public:
    explicit WorldModel(QObject* parent = nullptr);

    void reset();
    void applyUpdate(std::uint32_t seqNum,
                     bool fullSnapshot,
                     std::int32_t chunkX,
                     std::int32_t chunkY,
                     const QVector<AliveCell>& updates);

    [[nodiscard]] QVector<AliveCell> visibleCells(const QRectF& sceneRect) const;
    [[nodiscard]] QVector<QPair<std::uint64_t, std::size_t>> ownerStats() const;
    [[nodiscard]] std::size_t liveCellsOwnedBy(std::uint64_t ownerId) const;
    [[nodiscard]] std::uint64_t ownerAt(std::int64_t x, std::int64_t y) const;
    [[nodiscard]] std::uint32_t generation() const noexcept { return m_generation; }
    [[nodiscard]] std::size_t liveCellsCount() const noexcept { return m_aliveCells.size(); }

signals:
    void worldChanged();
    void statsChanged(std::uint32_t generation, std::size_t liveCells);

private:
    QHash<CellKey, std::uint64_t> m_aliveCells;
    std::uint32_t m_generation{0};
};
