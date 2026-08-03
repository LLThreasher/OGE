#pragma once
#include <array>
#include <cassert>
#include <cinttypes>

#include "oge/color.hpp"
#include "oge/runtime/net_serializer.hpp"
#include "oge/point3.hpp"

namespace game::terrain
{
namespace net = oge::runtime::net;

constexpr int CHUNK_SIZE_X = 16;
constexpr int CHUNK_SIZE_Y = 16;
constexpr int CHUNK_SIZE_Z = 16;
constexpr int CHUNK_SIZE_TOTAL = CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z;
constexpr int CHUNK_SHIFT_X = 0;
constexpr int CHUNK_SHIFT_Y = 4;
constexpr int CHUNK_SHIFT_Z = 8;

NET_OBJ(TerrainRendererDesc)
{
    net::UInt32 meshingQuadBudget = 4096 * 4;

    NET_OBJ_FN
    {
        visit(self.meshingQuadBudget);
    }
};

NET_OBJ(TerrainDesc)
{
    net::Int32 chunkViewDistance = 2;
    net::Int32 terrainGenChunkBudget = 8;

    NET_OBJ_FN
    {
        visit(self.chunkViewDistance);
        visit(self.terrainGenChunkBudget);
    }
};

namespace ChunkDir
{
    using oge::Point3;
    constexpr Point3 Left   {-1, 0, 0};
    constexpr Point3 Right  { 1, 0, 0};
    constexpr Point3 Up     { 0, 1, 0};
    constexpr Point3 Down   { 0,-1, 0};
    constexpr Point3 Front  { 0, 0, 1};
    constexpr Point3 Back   { 0, 0,-1};

    constexpr std::array<Point3, 6> All{
        Left, Right,
        Up, Down,
        Front, Back
    };

    constexpr std::array<Point3, 5> AllNoDown{
        Left, Right,
        Up,
        Front, Back
    };

    template<typename Func>
    inline void ForEachNeighbor(const Point3& coord, Func&& fn)
    {
        for (const auto& dir : ChunkDir::All)
        {
            if (dir.y == -1 && coord.y == 0)
                continue;

            fn(coord + dir);
        }
    }
}

}  // namespace game::terrain
