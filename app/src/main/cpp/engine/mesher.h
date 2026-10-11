#pragma once
#include <vector>
#include "engine/world.h"

struct ChunkMesh {
    std::vector<float> vertices;  // per vertex: x y z r g b
    int quadCount = 0;
    int solidBlocks = 0;
};

struct FaceDef {
    int dx, dy, dz;    // direction of the neighbouring block
    float ox, oy, oz;  // face origin offset inside the block
    float ux, uy, uz;  // first edge
    float vx, vy, vz;  // second edge
    float shade;       // simple fake lighting until real lighting arrives (v0.7)
};

// Corners are origin, +u, +u+v, +v: counter-clockwise seen from outside the block.
inline const FaceDef kFaces[6] = {
    { 1,  0,  0,   1, 0, 0,   0, 1, 0,   0, 0, 1,   0.80f},  // +X
    {-1,  0,  0,   0, 0, 0,   0, 0, 1,   0, 1, 0,   0.80f},  // -X
    { 0,  1,  0,   0, 1, 0,   0, 0, 1,   1, 0, 0,   1.00f},  // +Y (top)
    { 0, -1,  0,   0, 0, 0,   1, 0, 0,   0, 0, 1,   0.50f},  // -Y (bottom)
    { 0,  0,  1,   0, 0, 1,   1, 0, 0,   0, 1, 0,   0.70f},  // +Z
    { 0,  0, -1,   0, 0, 0,   0, 1, 0,   1, 0, 0,   0.70f},  // -Z
};

// Builds one chunk's mesh. A face is emitted only when the block next to it is
// not solid. The neighbour lookup goes through the world, so chunk borders work.
inline ChunkMesh buildChunkMesh(const World& world, int cx, int cz) {
    ChunkMesh mesh;
    const Chunk& chunk = world.chunk(cx, cz);

    for (int y = 0; y < CHUNK_HEIGHT; ++y) {
        for (int z = 0; z < CHUNK_SIZE; ++z) {
            for (int x = 0; x < CHUNK_SIZE; ++x) {
                BlockId id = chunk.get(x, y, z);
                if (!BlockRegistry::isSolid(id)) continue;
                ++mesh.solidBlocks;

                const BlockDef& def = BlockRegistry::get(id);
                int wx = cx * CHUNK_SIZE + x;
                int wz = cz * CHUNK_SIZE + z;

                for (const FaceDef& f : kFaces) {
                    if (BlockRegistry::isSolid(world.getBlock(wx + f.dx, y + f.dy, wz + f.dz))) {
                        continue;
                    }
                    float r = def.r * f.shade;
                    float g = def.g * f.shade;
                    float b = def.b * f.shade;
                    float bx = (float)wx + f.ox;
                    float by = (float)y + f.oy;
                    float bz = (float)wz + f.oz;
                    const float ex[4] = {0.0f, f.ux, f.ux + f.vx, f.vx};
                    const float ey[4] = {0.0f, f.uy, f.uy + f.vy, f.vy};
                    const float ez[4] = {0.0f, f.uz, f.uz + f.vz, f.vz};
                    for (int i = 0; i < 4; ++i) {
                        mesh.vertices.push_back(bx + ex[i]);
                        mesh.vertices.push_back(by + ey[i]);
                        mesh.vertices.push_back(bz + ez[i]);
                        mesh.vertices.push_back(r);
                        mesh.vertices.push_back(g);
                        mesh.vertices.push_back(b);
                    }
                    ++mesh.quadCount;
                }
            }
        }
    }
    return mesh;
}
