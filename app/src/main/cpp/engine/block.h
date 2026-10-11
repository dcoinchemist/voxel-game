#pragma once
#include <cstdint>

using BlockId = uint16_t;

enum : BlockId {
    BLOCK_AIR = 0,
    BLOCK_GRASS = 1,
    BLOCK_DIRT = 2,
    BLOCK_STONE = 3,
    BLOCK_COUNT = 4
};

struct BlockDef {
    const char* name;
    bool solid;
    float r, g, b;  // flat color for now; textures arrive in v0.6
};

struct BlockRegistry {
    static const BlockDef& get(BlockId id) {
        static const BlockDef defs[BLOCK_COUNT] = {
            {"air",   false, 0.00f, 0.00f, 0.00f},
            {"grass", true,  0.30f, 0.65f, 0.25f},
            {"dirt",  true,  0.45f, 0.30f, 0.18f},
            {"stone", true,  0.50f, 0.50f, 0.52f},
        };
        return defs[id < BLOCK_COUNT ? id : 0];
    }
    static bool isSolid(BlockId id) { return get(id).solid; }
};
