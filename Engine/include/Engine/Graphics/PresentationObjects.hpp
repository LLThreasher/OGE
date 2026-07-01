#pragma once
#include <string>

#include "Engine/entt.hpp"
#include "Engine/Math.hpp"
#include "Engine/ObjectType.hpp"
#include "Engine/Point2.hpp"
#include "Engine/Point3.hpp"

namespace OneGame::Engine::Graphics
{
struct PDebugRect
{
    Point2 pos;
    UPoint2 extent;
    ColorRGBA8 color = COLOR_WHITE;
};

enum class GameViewType : uint32_t
{
    Overlay = 1 << 0,
    Slot0 = 1 << 1,
    Slot1 = 1 << 2,
    Slot2 = 1 << 3,
    Slot3 = 1 << 4,
};

struct PGameViewTag
{
    GameViewType type;
};

struct PGameView
{
    Point2 pos;
    UPoint2 extent;
    GameViewType type;
};

struct PViewTransform
{
    math::mat4 view;
};

struct PPerspectiveTransform
{
    float fov;
    float aspect;
};

// always drawn on top left corner
struct PDebugText
{
    std::string text;
};

struct PTerrainMesh
{
    GPUChunkedAllocation alloc;
    uint32_t indexCount;
};
}  // namespace OneGame::Engine::Graphics
