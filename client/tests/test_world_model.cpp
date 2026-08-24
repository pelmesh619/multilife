#include "WorldModel.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>

namespace {

bool hasCell(const QVector<AliveCell>& cells, std::int64_t x, std::int64_t y, std::uint64_t owner)
{
    return std::any_of(cells.begin(), cells.end(), [x, y, owner](const AliveCell& cell) {
        return cell.x == x && cell.y == y && cell.owner == owner;
    });
}

} // namespace

TEST(WorldModelTest, ResetClearsStateAndEmitsSignals)
{
    WorldModel model;
    int worldChangedCount = 0;
    int statsChangedCount = 0;

    QObject::connect(&model, &WorldModel::worldChanged, [&]() { ++worldChangedCount; });
    QObject::connect(
        &model,
        &WorldModel::statsChanged,
        [&](std::uint32_t, std::size_t) { ++statsChangedCount; });

    model.applyUpdate(7, false, 0, 0, {{1, 2, 42}});
    EXPECT_EQ(model.generation(), static_cast<std::uint32_t>(7));
    EXPECT_EQ(model.liveCellsCount(), static_cast<std::size_t>(1));

    model.reset();

    EXPECT_EQ(model.generation(), static_cast<std::uint32_t>(0));
    EXPECT_EQ(model.liveCellsCount(), static_cast<std::size_t>(0));
    EXPECT_GE(worldChangedCount, 2);
    EXPECT_GE(statsChangedCount, 2);
}

TEST(WorldModelTest, ApplyDeltaAddsAndRemovesCells)
{
    WorldModel model;

    model.applyUpdate(1, false, 0, 0, {
        {0, 0, 11},
        {3, 5, 11}
    });
    EXPECT_EQ(model.generation(), static_cast<std::uint32_t>(1));
    EXPECT_EQ(model.liveCellsCount(), static_cast<std::size_t>(2));

    model.applyUpdate(2, false, 0, 0, {
        {0, 0, 0},
        {7, 9, 99}
    });
    EXPECT_EQ(model.generation(), static_cast<std::uint32_t>(2));
    EXPECT_EQ(model.liveCellsCount(), static_cast<std::size_t>(2));

    const auto cells = model.visibleCells(QRectF(-10, -10, 64, 64));
    EXPECT_FALSE(hasCell(cells, 0, 0, 11));
    EXPECT_TRUE(hasCell(cells, 3, 5, 11));
    EXPECT_TRUE(hasCell(cells, 7, 9, 99));
}

TEST(WorldModelTest, FullSnapshotReplacesOnlyTargetChunk)
{
    WorldModel model;

    model.applyUpdate(1, false, 0, 0, {{5, 5, 1}});
    model.applyUpdate(1, false, 1, 0, {{2, 2, 2}});
    EXPECT_EQ(model.liveCellsCount(), static_cast<std::size_t>(2));

    model.applyUpdate(2, true, 0, 0, {{10, 10, 3}});
    EXPECT_EQ(model.generation(), static_cast<std::uint32_t>(2));
    EXPECT_EQ(model.liveCellsCount(), static_cast<std::size_t>(2));

    const auto cells = model.visibleCells(QRectF(-5, -5, 140, 80));
    EXPECT_FALSE(hasCell(cells, 5, 5, 1));
    EXPECT_TRUE(hasCell(cells, 10, 10, 3));
    EXPECT_TRUE(hasCell(cells, 64 + 2, 2, 2));
}

TEST(WorldModelTest, VisibleCellsReturnsOnlyRequestedRect)
{
    WorldModel model;

    model.applyUpdate(1, false, -1, 0, {{63, 2, 8}});
    model.applyUpdate(1, false, 0, 0, {{10, 10, 9}});
    model.applyUpdate(1, false, 2, -1, {{1, 1, 7}});

    const auto nearOrigin = model.visibleCells(QRectF(0, 0, 20, 20));
    EXPECT_EQ(nearOrigin.size(), 1);
    EXPECT_TRUE(hasCell(nearOrigin, 10, 10, 9));

    const auto leftChunk = model.visibleCells(QRectF(-4, 0, 8, 8));
    EXPECT_EQ(leftChunk.size(), 1);
    EXPECT_TRUE(hasCell(leftChunk, -1, 2, 8));
}

TEST(WorldModelTest, OwnerStatsAreSortedByLiveCount)
{
    WorldModel model;

    model.applyUpdate(1, false, 0, 0, {
        {0, 0, 1},
        {1, 0, 1},
        {2, 0, 1},
        {3, 0, 2},
        {4, 0, 2},
        {5, 0, 3}
    });

    const auto stats = model.ownerStats();
    ASSERT_EQ(stats.size(), 3);
    EXPECT_EQ(stats[0].first, static_cast<std::uint64_t>(1));
    EXPECT_EQ(stats[0].second, static_cast<std::size_t>(3));
    EXPECT_EQ(stats[1].first, static_cast<std::uint64_t>(2));
    EXPECT_EQ(stats[1].second, static_cast<std::size_t>(2));
    EXPECT_EQ(stats[2].first, static_cast<std::uint64_t>(3));
    EXPECT_EQ(stats[2].second, static_cast<std::size_t>(1));

    EXPECT_EQ(model.liveCellsOwnedBy(1), static_cast<std::size_t>(3));
    EXPECT_EQ(model.liveCellsOwnedBy(2), static_cast<std::size_t>(2));
    EXPECT_EQ(model.liveCellsOwnedBy(3), static_cast<std::size_t>(1));
    EXPECT_EQ(model.liveCellsOwnedBy(99), static_cast<std::size_t>(0));
}
