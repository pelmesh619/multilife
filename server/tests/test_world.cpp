#include <gtest/gtest.h>
#include "World.hpp"
#include "Types.hpp"
#include "PlayerCommand.hpp"

using namespace multilife;


// Chunk creation tests
TEST(WorldTest, GetOrCreateChunkReturnsChunk) {
    World world;
    ChunkCoord coord{0, 0};
    Chunk& chunk = world.getOrCreateChunk(coord);
    EXPECT_FALSE(chunk.getCell(0, 0).alive);
}

TEST(WorldTest, GetOrCreateChunkReturnsSameChunkOnRepeatCalls) {
    World world;
    ChunkCoord coord{1, 2};
    Chunk& a = world.getOrCreateChunk(coord);
    Chunk& b = world.getOrCreateChunk(coord);
    EXPECT_EQ(&a, &b);
}

TEST(WorldTest, TryGetChunkReturnsNullForMissingChunk) {
    World world;
    const Chunk* chunk = world.tryGetChunk({99, 99});
    EXPECT_EQ(chunk, nullptr);
}

TEST(WorldTest, TryGetChunkReturnsChunkAfterCreation) {
    World world;
    ChunkCoord coord{3, 3};
    world.getOrCreateChunk(coord);
    const Chunk* chunk = world.tryGetChunk(coord);
    EXPECT_NE(chunk, nullptr);
}

// applyCommands: PlaceCell test
TEST(WorldTest, PlaceCellCommandMakesCellAlive) {
    World world;
    PlayerCommand cmd{1, CommandType::PlaceCell, 0, 0};
    world.applyCommands({cmd});
    const Chunk* chunk = world.tryGetChunk({0, 0});
    ASSERT_NE(chunk, nullptr);
    EXPECT_TRUE(chunk->getCell(0, 0).alive);
    EXPECT_EQ(chunk->getCell(0, 0).owner, 1u);
}

TEST(WorldTest, PlaceCellCommandCreatesChunkIfNeeded) {
    World world;
    EXPECT_EQ(world.tryGetChunk({5, 5}), nullptr);
    PlayerCommand cmd{1, CommandType::PlaceCell, 5 * ChunkWidth, 5 * ChunkHeight};
    world.applyCommands({cmd});
    EXPECT_NE(world.tryGetChunk({5, 5}), nullptr);
}

// applyCommands: RemoveCell test
TEST(WorldTest, RemoveCellCommandKillsCell) {
    World world;
    world.applyCommands({{1, CommandType::PlaceCell, 10, 10}});
    world.applyCommands({{1, CommandType::RemoveCell, 10, 10}});
    const Chunk* chunk = world.tryGetChunk({0, 0});
    ASSERT_NE(chunk, nullptr);
    EXPECT_FALSE(chunk->getCell(10, 10).alive);
}

// applyCommands: ToggleCell

TEST(WorldTest, ToggleCellTurnsDeadCellAlive) {
    World world;
    world.applyCommands({{1, CommandType::ToggleCell, 3, 3}});
    EXPECT_TRUE(world.tryGetChunk({0, 0})->getCell(3, 3).alive);
}

TEST(WorldTest, ToggleCellTurnsAliveCellDead) {
    World world;
    world.applyCommands({{1, CommandType::PlaceCell, 3, 3}});
    world.applyCommands({{1, CommandType::ToggleCell, 3, 3}});
    EXPECT_FALSE(world.tryGetChunk({0, 0})->getCell(3, 3).alive);
}

// Coordinate mapping across chunk boundaries

TEST(WorldTest, CommandAtChunkBoundaryRoutesToCorrectChunk) {
    World world;
    PlayerCommand cmd{1, CommandType::PlaceCell, (std::int64_t)ChunkWidth, 0};
    world.applyCommands({cmd});
    EXPECT_EQ(world.tryGetChunk({0, 0}), nullptr);  // chunk (0,0) should NOT be created
    const Chunk* c10 = world.tryGetChunk({1, 0});
    ASSERT_NE(c10, nullptr);
    EXPECT_TRUE(c10->getCell(0, 0).alive);
}

TEST(WorldTest, NegativeCoordinatesMapToCorrectChunk) {
    World world;
    PlayerCommand cmd{1, CommandType::PlaceCell, -1, -1};
    world.applyCommands({cmd});
    const Chunk* chunk = world.tryGetChunk({-1, -1});
    ASSERT_NE(chunk, nullptr);
    EXPECT_TRUE(chunk->getCell(ChunkWidth - 1, ChunkHeight - 1).alive);
}

// allChunks tests

TEST(WorldTest, AllChunksReturnsAllCreatedChunks) {
    World world;
    world.getOrCreateChunk({0, 0});
    world.getOrCreateChunk({1, 0});
    world.getOrCreateChunk({0, 1});
    auto chunks = world.allChunks();
    EXPECT_EQ(chunks.size(), 3u);
}

TEST(WorldTest, AllChunksReturnsEmptyWhenWorldIsEmpty) {
    World world;
    EXPECT_TRUE(world.allChunks().empty());
}

// Batch commands test

TEST(WorldTest, BatchCommandsAllApplied) {
    World world;
    std::vector<PlayerCommand> batch = {
        {1, CommandType::PlaceCell, 0, 0},
        {2, CommandType::PlaceCell, 1, 0},
        {3, CommandType::PlaceCell, 2, 0},
    };
    world.applyCommands(batch);
    const Chunk* chunk = world.tryGetChunk({0, 0});
    ASSERT_NE(chunk, nullptr);
    EXPECT_EQ(chunk->getCell(0, 0).owner, 1u);
    EXPECT_EQ(chunk->getCell(1, 0).owner, 2u);
    EXPECT_EQ(chunk->getCell(2, 0).owner, 3u);
}
