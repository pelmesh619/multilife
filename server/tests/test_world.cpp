#include <gtest/gtest.h>
#include "World.hpp"
#include "Types.hpp"
#include "PlayerCommand.hpp"

using namespace multilife;


// Chunk creation tests
TEST(WorldTest, GetOrCreateChunkReturnsChunk) {
    ResourceManager rm;
    World world(rm);
    ChunkCoord coord{0, 0};
    Chunk& chunk = world.getOrCreateChunk(coord);
    EXPECT_FALSE(chunk.getCell(0, 0).alive);
}

TEST(WorldTest, GetOrCreateChunkReturnsSameChunkOnRepeatCalls) {
    ResourceManager rm;
    World world(rm);
    ChunkCoord coord{1, 2};
    Chunk& a = world.getOrCreateChunk(coord);
    Chunk& b = world.getOrCreateChunk(coord);
    EXPECT_EQ(&a, &b);
}

TEST(WorldTest, TryGetChunkReturnsNullForMissingChunk) {
    ResourceManager rm;
    World world(rm);
    const Chunk* chunk = world.tryGetChunk({99, 99});
    EXPECT_EQ(chunk, nullptr);
}

TEST(WorldTest, TryGetChunkReturnsChunkAfterCreation) {
    ResourceManager rm;
    World world(rm);
    ChunkCoord coord{3, 3};
    world.getOrCreateChunk(coord);
    const Chunk* chunk = world.tryGetChunk(coord);
    EXPECT_NE(chunk, nullptr);
}

// applyCommands: PlaceCell test
TEST(WorldTest, PlaceCellCommandMakesCellAlive) {
    ResourceManager rm;
    World world(rm);
    rm.addPlayer(1);
    PlayerCommand cmd{1, CommandType::PlaceCell, 0, 0};

    world.applyCommands({cmd});
    const Chunk* chunk = world.tryGetChunk({0, 0});
    ASSERT_NE(chunk, nullptr);
    EXPECT_TRUE(chunk->getCell(0, 0).alive);
    EXPECT_EQ(chunk->getCell(0, 0).owner, 1u);
}

TEST(WorldTest, PlaceCellCommandCreatesChunkIfNeeded) {
    ResourceManager rm;
    World world(rm);
    rm.addPlayer(1);
    EXPECT_EQ(world.tryGetChunk({5, 5}), nullptr);
    PlayerCommand cmd{1, CommandType::PlaceCell, 5 * ChunkWidth, 5 * ChunkHeight};

    world.applyCommands({cmd});
    EXPECT_NE(world.tryGetChunk({5, 5}), nullptr);
}

// applyCommands: RemoveCell test
TEST(WorldTest, RemoveCellCommandKillsCell) {
    ResourceManager rm;
    World world(rm);
    rm.addPlayer(1);
    world.applyCommands({{1, CommandType::PlaceCell, 10, 10}});
    world.applyCommands({{1, CommandType::RemoveCell, 10, 10}});

    const Chunk* chunk = world.tryGetChunk({0, 0});
    ASSERT_NE(chunk, nullptr);
    EXPECT_FALSE(chunk->getCell(10, 10).alive);
}

// applyCommands: ToggleCell

TEST(WorldTest, ToggleCellTurnsDeadCellAlive) {
    ResourceManager rm;
    World world(rm);
    rm.addPlayer(1);
    world.applyCommands({{1, CommandType::ToggleCell, 3, 3}});
    EXPECT_TRUE(world.tryGetChunk({0, 0})->getCell(3, 3).alive);
}

TEST(WorldTest, ToggleCellTurnsAliveCellDead) {
    ResourceManager rm;
    World world(rm);
    rm.addPlayer(1);
    world.applyCommands({{1, CommandType::PlaceCell, 3, 3}});
    world.applyCommands({{1, CommandType::ToggleCell, 3, 3}});
    EXPECT_FALSE(world.tryGetChunk({0, 0})->getCell(3, 3).alive);
}

// Coordinate mapping across chunk boundaries

TEST(WorldTest, CommandAtChunkBoundaryRoutesToCorrectChunk) {
    ResourceManager rm;
    World world(rm);
    rm.addPlayer(1);
    PlayerCommand cmd{1, CommandType::PlaceCell, (std::int64_t)ChunkWidth, 0};
    world.applyCommands({cmd});
    EXPECT_EQ(world.tryGetChunk({0, 0}), nullptr);  // chunk (0,0) should NOT be created
    const Chunk* c10 = world.tryGetChunk({1, 0});
    ASSERT_NE(c10, nullptr);
    EXPECT_TRUE(c10->getCell(0, 0).alive);
}

TEST(WorldTest, NegativeCoordinatesMapToCorrectChunk) {
    ResourceManager rm;
    World world(rm);
    rm.addPlayer(1);
    PlayerCommand cmd{1, CommandType::PlaceCell, -1, -1};
    world.applyCommands({cmd});
    const Chunk* chunk = world.tryGetChunk({-1, -1});
    ASSERT_NE(chunk, nullptr);
    EXPECT_TRUE(chunk->getCell(ChunkWidth - 1, ChunkHeight - 1).alive);
}

// allChunks tests

TEST(WorldTest, AllChunksReturnsAllCreatedChunks) {
    ResourceManager rm;
    World world(rm);
    rm.addPlayer(1);
    world.getOrCreateChunk({0, 0});
    world.getOrCreateChunk({1, 0});
    world.getOrCreateChunk({0, 1});
    auto chunks = world.allChunks();
    EXPECT_EQ(chunks.size(), 3u);
}

TEST(WorldTest, AllChunksReturnsEmptyWhenWorldIsEmpty) {
    ResourceManager rm;
    World world(rm);
    rm.addPlayer(1);
    EXPECT_TRUE(world.allChunks().empty());
}

// Batch commands test

TEST(WorldTest, BatchCommandsAllApplied) {
    ResourceManager rm;
    World world(rm);
    rm.addPlayer(1);
    rm.addPlayer(2);
    rm.addPlayer(3);
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
