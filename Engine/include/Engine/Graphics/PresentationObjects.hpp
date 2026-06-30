#pragma once
#include <string>

#include "Engine/entt.hpp"
#include "Engine/Math.hpp"
#include "Engine/ObjectType.hpp"
#include "Engine/Point3.hpp"

namespace OneGame::Engine::Graphics
{
struct PRect
{
    int32_t posX;
    int32_t posY;
    uint32_t extentX;
    uint32_t extentY;
};

struct PDefaultGameView
{
static entt::entity Get(const entt::registry& world)
{
    auto view = world.view<PDefaultGameView>();
    assert(!view.empty());
    return view.front();
}
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
