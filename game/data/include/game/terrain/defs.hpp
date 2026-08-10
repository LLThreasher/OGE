#pragma once
#include <array>
#include <cassert>
#include <cstdint>

#include "oge/point3.hpp"

namespace game::terrain
{

constexpr int CHUNK_SIZE_X = 16;
constexpr int CHUNK_SIZE_Y = 16;
constexpr int CHUNK_SIZE_Z = 16;
constexpr int CHUNK_SIZE_TOTAL = CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z;
constexpr int CHUNK_SHIFT_X = 0;
constexpr int CHUNK_SHIFT_Y = 4;
constexpr int CHUNK_SHIFT_Z = 8;

struct TerrainRendererDesc
{
    uint32_t meshingQuadBudget = 4096 * 4;
};

struct TerrainDesc
{
    int32_t chunkViewDistance = 8;
    int32_t terrainGenChunkBudget = 8;
};

namespace ChunkDir
{
using oge::Point3;
constexpr uint8_t FACE_IDX_LEFT = 0;
constexpr uint8_t FACE_IDX_RIGHT = 1;
constexpr uint8_t FACE_IDX_UP = 2;
constexpr uint8_t FACE_IDX_DOWN = 3;
constexpr uint8_t FACE_IDX_FRONT = 4;
constexpr uint8_t FACE_IDX_BACK = 5;

constexpr uint8_t FACE_MASK_LEFT = 1 << 0;
constexpr uint8_t FACE_MASK_RIGHT = 1 << 1;
constexpr uint8_t FACE_MASK_UP = 1 << 2;
constexpr uint8_t FACE_MASK_DOWN = 1 << 3;
constexpr uint8_t FACE_MASK_FRONT = 1 << 4;
constexpr uint8_t FACE_MASK_BACK = 1 << 5;

constexpr Point3 Left{-1, 0, 0};
constexpr Point3 Right{1, 0, 0};
constexpr Point3 Up{0, 1, 0};
constexpr Point3 Down{0, -1, 0};
constexpr Point3 Front{0, 0, 1};
constexpr Point3 Back{0, 0, -1};

constexpr std::array<Point3, 6> All{Left, Right, Up, Down, Front, Back};

constexpr std::array<Point3, 5> AllNoDown{Left, Right, Up, Front, Back};

template <typename Func>
inline void ForEachNeighbor(const Point3& coord, Func&& fn)
{
    for (const auto& dir : ChunkDir::All)
    {
        if (dir.y == -1 && coord.y == 0) continue;

        fn(coord + dir);
    }
}

template <typename Func>
inline void ForEachDirtyChunkNeighborIdx(const Point3& block,
                                         Func&& handleChunk)
{
    int32_t x = block.x;
    int32_t y = block.y;
    int32_t z = block.z;

    Point3 chunkCoord{block.x >> 4, block.y >> 4, block.z >> 4};

    if ((x & 0xF) == 0)
    {
        handleChunk(FACE_IDX_LEFT);
    }
    else if ((x & 0xF) == 15)
    {
        handleChunk(FACE_IDX_RIGHT);
    }
    if ((y & 0xF) == 0)
    {
        handleChunk(FACE_IDX_DOWN);
    }
    else if ((y & 0xF) == 15)
    {
        handleChunk(FACE_IDX_UP);
    }
    if ((z & 0xF) == 0)
    {
        handleChunk(FACE_IDX_BACK);
    }
    else if ((z & 0xF) == 15)
    {
        handleChunk(FACE_IDX_FRONT);
    }
}

template <typename Func>
inline void ForEachDirtyChunkNeighbor(const Point3& block, Func&& handleChunk)
{
    ForEachDirtyChunkNeighborIdx(
        block,
        [&](auto face)
        {
            handleChunk(All[face] +
                        Point3{block.x >> 4, block.y >> 4, block.z >> 4});
        });
}
}  // namespace ChunkDir

}  // namespace game::terrain
