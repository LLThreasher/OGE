#pragma once
#include <array>
#include <cassert>
#include <cinttypes>

#include "oge/color.hpp"
#include "oge/runtime/net_serializer.hpp"

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
        visit(meshingQuadBudget);
    }
};

NET_OBJ(TerrainDesc)
{
    net::Int32 chunkViewDistance = 8;
    net::Int32 terrainGenChunkBudget = 8;

    NET_OBJ_FN
    {
        visit(chunkViewDistance);
        visit(terrainGenChunkBudget);
    }
};

}  // namespace OneGame::Engine::Terrain
