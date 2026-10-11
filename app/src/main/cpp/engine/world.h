#pragma once
#include <cmath>
#include <vector>
#include "engine/block.h"

constexpr int CHUNK_SIZE = 16;
constexpr int CHUNK_HEIGHT = 32;
constexpr int WORLD_CHUNKS = 4;  // 4 x 4 chunks
constexpr int WORLD_SIZE = WORLD_CHUNKS * CHUNK_SIZE;

struct Chunk {
    std::vector<BlockId> blocks;

    Chunk() : blocks(CHUNK_SIZE * CHUNK_HEIGHT * CHUNK_SIZE, BLOCK_AIR) {}

    static int index(int x, int y, int z) { return (y * CHUNK_SIZE + z) * CHUNK_SIZE + x; }
    BlockId get(int x, int y, int z) const { return blocks[index(x, y, z)]; }
    void set(int x, int y, int z, BlockId id) { blocks[index(x, y, z)] = id; }
};

class World {
public:
    World() : chunks_(WORLD_CHUNKS * WORLD_CHUNKS) {}

    const Chunk& chunk(int cx, int cz) const { return chunks_[cz * WORLD_CHUNKS + cx]; }

    // World-space block lookup. Outside the world: air to the sides and above,
    // solid below, so the underside of the world is never drawn.
    BlockId getBlock(int wx, int wy, int wz) const {
        if (wy < 0) return BLOCK_STONE;
        if (wy >= CHUNK_HEIGHT) return BLOCK_AIR;
        if (wx < 0 || wz < 0 || wx >= WORLD_SIZE || wz >= WORLD_SIZE) return BLOCK_AIR;
        const Chunk& c = chunk(wx / CHUNK_SIZE, wz / CHUNK_SIZE);
        return c.get(wx % CHUNK_SIZE, wy, wz % CHUNK_SIZE);
    }

    void setBlock(int wx, int wy, int wz, BlockId id) {
        if (wy < 0 || wy >= CHUNK_HEIGHT) return;
        if (wx < 0 || wz < 0 || wx >= WORLD_SIZE || wz >= WORLD_SIZE) return;
        chunks_[(wz / CHUNK_SIZE) * WORLD_CHUNKS + (wx / CHUNK_SIZE)]
            .set(wx % CHUNK_SIZE, wy, wz % CHUNK_SIZE, id);
    }

    // Simple hand-made hills, just to give the mesher something to chew on.
    void generate() {
        for (int wz = 0; wz < WORLD_SIZE; ++wz) {
            for (int wx = 0; wx < WORLD_SIZE; ++wx) {
                float fx = (float)wx;
                float fz = (float)wz;
                int h = 12 + (int)(5.0f * std::sin(fx * 0.21f) + 4.0f * std::cos(fz * 0.17f) +
                                   2.0f * std::sin((fx + fz) * 0.11f));
                if (h < 3) h = 3;
                if (h > CHUNK_HEIGHT - 2) h = CHUNK_HEIGHT - 2;
                for (int y = 0; y <= h; ++y) {
                    BlockId id = (y == h) ? BLOCK_GRASS : (y > h - 4 ? BLOCK_DIRT : BLOCK_STONE);
                    setBlock(wx, y, wz, id);
                }
            }
        }
        // A tunnel through the hills, so cave walls test the culling too.
        for (int wx = 8; wx < 56; ++wx) {
            for (int wz = 30; wz <= 32; ++wz) {
                for (int y = 6; y <= 8; ++y) {
                    setBlock(wx, y, wz, BLOCK_AIR);
                }
            }
        }
    }

private:
    std::vector<Chunk> chunks_;
};
