#include <gtest/gtest.h>
#include "Chunk.hpp"
#include "Types.hpp"

using namespace multilife;


// Helper functions
static void setAlive(Chunk& chunk, std::size_t x, std::size_t y, PlayerId owner = 1) {
    chunk.setCell(x, y, CellState{true, owner});
}

static bool isAlive(const Chunk& chunk, std::size_t x, std::size_t y) {
    return chunk.getCell(x, y).alive;
}


// Initialization tests

TEST(ChunkTest, NewChunkIsAllDead) {
    Chunk chunk;
    for (std::size_t y = 0; y < ChunkHeight; ++y)
        for (std::size_t x = 0; x < ChunkWidth; ++x)
            EXPECT_FALSE(isAlive(chunk, x, y))
                << "Expected dead cell at (" << x << ", " << y << ")";
}

TEST(ChunkTest, ClearResetsAllCells) {
    Chunk chunk;
    setAlive(chunk, 0, 0);
    setAlive(chunk, 10, 10);
    chunk.clear();
    for (std::size_t y = 0; y < ChunkHeight; ++y)
        for (std::size_t x = 0; x < ChunkWidth; ++x)
            EXPECT_FALSE(isAlive(chunk, x, y));
}

// Conway's Game rules test

// Live cell with < 2 live neighbors dies
TEST(ChunkTest, LiveCellWithOneNeighborDies) {
    Chunk chunk;
    setAlive(chunk, 5, 5);
    setAlive(chunk, 5, 6);
    chunk.calculateNext();
    chunk.swapBuffers();
    EXPECT_FALSE(isAlive(chunk, 5, 5));
}

// Live cell with 2 or 3 neighbors survives
TEST(ChunkTest, LiveCellWithTwoNeighborsSurvives) {
    Chunk chunk;
    setAlive(chunk, 5, 5);
    setAlive(chunk, 6, 5);
    setAlive(chunk, 7, 5);
    chunk.calculateNext();
    chunk.swapBuffers();
    EXPECT_TRUE(isAlive(chunk, 6, 5));  // center survives
}

TEST(ChunkTest, LiveCellWithThreeNeighborsSurvives) {
    Chunk chunk;
    setAlive(chunk, 5, 5);
    setAlive(chunk, 6, 5);
    setAlive(chunk, 5, 6);
    setAlive(chunk, 6, 6);
    chunk.calculateNext();
    chunk.swapBuffers();
    EXPECT_TRUE(isAlive(chunk, 5, 5));
    EXPECT_TRUE(isAlive(chunk, 6, 5));
    EXPECT_TRUE(isAlive(chunk, 5, 6));
    EXPECT_TRUE(isAlive(chunk, 6, 6));
}

// Live cell with > 3 neighbors dies
TEST(ChunkTest, LiveCellWithFourNeighborsDies) {
    Chunk chunk;
    setAlive(chunk, 5, 5);  // center
    setAlive(chunk, 4, 5);
    setAlive(chunk, 6, 5);
    setAlive(chunk, 5, 4);
    setAlive(chunk, 5, 6);
    chunk.calculateNext();
    chunk.swapBuffers();
    EXPECT_FALSE(isAlive(chunk, 5, 5));
}

// Dead cell with exactly 3 neighbors becomes alive
TEST(ChunkTest, DeadCellWithThreeNeighborsBecomeAlive) {
    Chunk chunk;
    setAlive(chunk, 4, 5);
    setAlive(chunk, 6, 5);
    setAlive(chunk, 5, 4);
    // (5,5) is dead with 3 live neighbors
    chunk.calculateNext();
    chunk.swapBuffers();
    EXPECT_TRUE(isAlive(chunk, 5, 5));
}

TEST(ChunkTest, DeadCellWithTwoNeighborsStaysDead) {
    Chunk chunk;
    setAlive(chunk, 4, 5);
    setAlive(chunk, 6, 5);
    // (5,5) is dead with only 2 neighbors — stays dead
    chunk.calculateNext();
    chunk.swapBuffers();
    EXPECT_FALSE(isAlive(chunk, 5, 5));
}

// Stable and oscillating patterns tests

// Block 2x2
TEST(ChunkTest, BlockPatternIsStable) {
    Chunk chunk;
    setAlive(chunk, 10, 10);
    setAlive(chunk, 11, 10);
    setAlive(chunk, 10, 11);
    setAlive(chunk, 11, 11);
    chunk.calculateNext();
    chunk.swapBuffers();
    EXPECT_TRUE(isAlive(chunk, 10, 10));
    EXPECT_TRUE(isAlive(chunk, 11, 10));
    EXPECT_TRUE(isAlive(chunk, 10, 11));
    EXPECT_TRUE(isAlive(chunk, 11, 11));
}

// Oscillator
TEST(ChunkTest, BlinkerOscillatesCorrectly) {
    Chunk chunk;
    // Horizontal row
    setAlive(chunk, 5, 5);
    setAlive(chunk, 6, 5);
    setAlive(chunk, 7, 5);

    chunk.calculateNext();
    chunk.swapBuffers();

    // Should become vertical
    EXPECT_FALSE(isAlive(chunk, 5, 5));
    EXPECT_TRUE(isAlive(chunk, 6, 4));
    EXPECT_TRUE(isAlive(chunk, 6, 5));
    EXPECT_TRUE(isAlive(chunk, 6, 6));

    chunk.calculateNext();
    chunk.swapBuffers();

    // Should revert to horizontal
    EXPECT_TRUE(isAlive(chunk, 5, 5));
    EXPECT_TRUE(isAlive(chunk, 6, 5));
    EXPECT_TRUE(isAlive(chunk, 7, 5));
}


// Double buffering tests

TEST(ChunkTest, CalculateNextDoesNotModifyCurrentBuffer) {
    Chunk chunk;
    setAlive(chunk, 5, 5);
    setAlive(chunk, 6, 5);
    setAlive(chunk, 7, 5);

    chunk.calculateNext();

    // Current buffer must be unchanged
    EXPECT_TRUE(isAlive(chunk, 5, 5));
    EXPECT_TRUE(isAlive(chunk, 6, 5));
    EXPECT_TRUE(isAlive(chunk, 7, 5));
}

TEST(ChunkTest, SwapBuffersCommitsNextGeneration) {
    Chunk chunk;
    setAlive(chunk, 5, 5);
    setAlive(chunk, 6, 5);
    setAlive(chunk, 7, 5);

    chunk.calculateNext();
    chunk.swapBuffers();

    // After swap, the new generation should be active
    EXPECT_FALSE(isAlive(chunk, 5, 5));
    EXPECT_TRUE(isAlive(chunk, 6, 4));
    EXPECT_TRUE(isAlive(chunk, 6, 5));
    EXPECT_TRUE(isAlive(chunk, 6, 6));
}


// Ghost cells tests

TEST(ChunkTest, GhostCellsCanBeSetAndRead) {
    Chunk chunk;
    CellState state{true, 42};
    // storageX=0, storageY=0 is the top-left ghost corner
    chunk.setGhostCell(0, 0, state);
    auto result = chunk.getGhostCell(0, 0);
    EXPECT_TRUE(result.alive);
    EXPECT_EQ(result.owner, 42u);
}

// getLiveCountByPlayer test

TEST(ChunkTest, LiveCountByPlayerIsEmptyWhenNoCells) {
    Chunk chunk;
    auto counts = chunk.getLiveCountByPlayer();
    EXPECT_TRUE(counts.empty());
}

TEST(ChunkTest, LiveCountByPlayerIgnoresOwnerZero) {
    Chunk chunk;
    chunk.setCell(5, 5, CellState{true, 0});  // 0 = no owner
    auto counts = chunk.getLiveCountByPlayer();
    EXPECT_EQ(counts.count(0), 0u);
}

TEST(ChunkTest, LiveCountByPlayerCountsCorrectly) {
    Chunk chunk;
    setAlive(chunk, 1, 1, 1);
    setAlive(chunk, 2, 2, 1);
    setAlive(chunk, 3, 3, 2);
    auto counts = chunk.getLiveCountByPlayer();
    EXPECT_EQ(counts[1], 2u);
    EXPECT_EQ(counts[2], 1u);
}

TEST(ChunkTest, LiveCountByPlayerIgnoresDeadCells) {
    Chunk chunk;
    chunk.setCell(5, 5, CellState{false, 1});  // dead but owned
    auto counts = chunk.getLiveCountByPlayer();
    EXPECT_EQ(counts.count(1), 0u);
}
