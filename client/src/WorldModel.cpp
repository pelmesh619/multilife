#include "WorldModel.h"

#include "ClientProtocol.h"

#include <QtMath>

#include <algorithm>

namespace {

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

uint qHash(const CellKey& key, uint seed) noexcept
{
    const auto x = static_cast<quint64>(key.x);
    const auto y = static_cast<quint64>(key.y);
    return qHash((x << 1U) ^ (y << 33U), seed);
}

WorldModel::WorldModel(QObject* parent)
    : QObject(parent)
{}

void WorldModel::reset()
{
    m_aliveCells.clear();
    m_generation = 0;
    emit worldChanged();
    emit statsChanged(m_generation, m_aliveCells.size());
}

void WorldModel::applyUpdate(std::uint32_t seqNum,
                             bool fullSnapshot,
                             std::int32_t chunkX,
                             std::int32_t chunkY,
                             const QVector<AliveCell>& updates)
{
    if (fullSnapshot) {
        auto it = m_aliveCells.begin();
        while (it != m_aliveCells.end()) {
            const auto keyChunkX =
                floorDiv(it.key().x, multilife::client::proto::kChunkWidth);
            const auto keyChunkY =
                floorDiv(it.key().y, multilife::client::proto::kChunkHeight);
            if (keyChunkX == chunkX && keyChunkY == chunkY) {
                it = m_aliveCells.erase(it);
            } else {
                ++it;
            }
        }
    }

    const std::int64_t baseX =
        static_cast<std::int64_t>(chunkX) * multilife::client::proto::kChunkWidth;
    const std::int64_t baseY =
        static_cast<std::int64_t>(chunkY) * multilife::client::proto::kChunkHeight;

    for (const auto& update : updates) {
        const CellKey key{
            baseX + update.x,
            baseY + update.y
        };
        if (update.owner == 0) {
            m_aliveCells.remove(key);
        } else {
            m_aliveCells.insert(key, update.owner);
        }
    }

    m_generation = seqNum;
    emit worldChanged();
    emit statsChanged(m_generation, m_aliveCells.size());
}

QVector<AliveCell> WorldModel::visibleCells(const QRectF& sceneRect) const
{
    QVector<AliveCell> result;
    result.reserve(static_cast<int>(std::min<std::size_t>(m_aliveCells.size(), 4096)));

    const auto left = static_cast<std::int64_t>(qFloor(sceneRect.left()));
    const auto top = static_cast<std::int64_t>(qFloor(sceneRect.top()));
    const auto right = static_cast<std::int64_t>(qCeil(sceneRect.right()));
    const auto bottom = static_cast<std::int64_t>(qCeil(sceneRect.bottom()));

    for (auto it = m_aliveCells.constBegin(); it != m_aliveCells.constEnd(); ++it) {
        const auto x = it.key().x;
        const auto y = it.key().y;
        if (x < left || x > right || y < top || y > bottom) {
            continue;
        }
        result.push_back({x, y, it.value()});
    }

    return result;
}

QVector<QPair<std::uint64_t, std::size_t>> WorldModel::ownerStats() const
{
    QHash<std::uint64_t, std::size_t> counts;
    for (auto it = m_aliveCells.constBegin(); it != m_aliveCells.constEnd(); ++it) {
        counts[it.value()] += 1;
    }

    QVector<QPair<std::uint64_t, std::size_t>> stats;
    stats.reserve(counts.size());
    for (auto it = counts.constBegin(); it != counts.constEnd(); ++it) {
        stats.push_back({it.key(), it.value()});
    }

    std::sort(stats.begin(), stats.end(), [](const auto& lhs, const auto& rhs) {
        if (lhs.second == rhs.second) {
            return lhs.first < rhs.first;
        }
        return lhs.second > rhs.second;
    });

    return stats;
}

std::size_t WorldModel::liveCellsOwnedBy(std::uint64_t ownerId) const
{
    std::size_t count = 0;
    for (auto it = m_aliveCells.constBegin(); it != m_aliveCells.constEnd(); ++it) {
        if (it.value() == ownerId) {
            count += 1;
        }
    }
    return count;
}

std::uint64_t WorldModel::ownerAt(std::int64_t x, std::int64_t y) const
{
    const CellKey key{x, y};
    const auto it = m_aliveCells.constFind(key);
    if (it == m_aliveCells.constEnd()) {
        return 0;
    }
    return it.value();
}
