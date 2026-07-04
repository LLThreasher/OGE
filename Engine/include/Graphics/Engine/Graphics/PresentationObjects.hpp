#pragma once
#include <string>

#include "Engine/entt.hpp"
#include "Engine/Math.hpp"
#include "Engine/ObjectType.hpp"
#include "Engine/Point2.hpp"
#include "Engine/Point3.hpp"
#include "Engine/Rect.hpp"

namespace OneGame::Engine::Graphics
{
struct PDebugRect : IRect
{
    ColorRGBA8 color = COLOR_WHITE;
};

enum class GameViewType : uint32_t
{
    Overlay = 1 << 0,
    Slot0 = 1 << 1,
    Slot1 = 1 << 2,
    Slot2 = 1 << 3,
    Slot3 = 1 << 4,
    All = Overlay | Slot0 | Slot1 | Slot2 | Slot3,
};

static constexpr std::array<GameViewType, 4> ALL_GAME_VIEWS = {
    GameViewType::Slot0,
    GameViewType::Slot1,
    GameViewType::Slot2,
    GameViewType::Slot3,
};

struct PGameViewTag
{
    GameViewType type;
};

struct PGameView : IRect
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

struct PGeneralMesh
{
    GPUBufferRange vertices;
    GPUBufferRange indices;
    uint32_t indexCount;
};
}  // namespace OneGame::Engine::Graphics
