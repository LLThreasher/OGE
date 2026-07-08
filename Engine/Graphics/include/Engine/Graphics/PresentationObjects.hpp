#pragma once
#include <string>

#include "Engine/Graphics/GPUObjects.hpp"
#include "Engine/Math.hpp"
#include "Engine/ObjectType.hpp"
#include "Engine/Point2.hpp"
#include "Engine/Point3.hpp"
#include "Engine/Rect.hpp"
#include "Engine/entt.hpp"

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

struct PSprite
{
    U16NormRect uv = {{0.f, 0.f}, {1.f, 1.f}};
    GPUTextureHandle texture;

    PSprite(GPUTextureHandle texture) : texture(texture) {}

    PSprite(GPUTextureRegion region, uint32_t total_width, uint32_t total_height)
    {
        float fwidth = total_width;
        float fheight = total_height;
        uv = {{(float)region.region.pos.x / fwidth, (float)region.region.pos.y / fheight},
              {(float)region.region.extent.x / fwidth, (float)region.region.extent.y / fheight}};
        texture = region.texture;
    }
};

struct PGlyph
{
    std::tuple<IRect16, PSprite> (*loc)(I16Point2 pos, std::string::iterator it);
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
