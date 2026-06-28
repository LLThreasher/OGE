#pragma once
#include <string>

#include "Engine/Math.hpp"

namespace OneGame::Engine::Graphics
{
struct PViewTransform
{
    math::mat4 view;
};

// always drawn on top left corner
struct PDebugText
{
    std::string text;
};

struct PTerrainMesh
{
    uint32_t offset;
    uint32_t indexCount;
    int chunkX, chunkY, chunkZ;
};
}  // namespace OneGame::Engine::Graphics
