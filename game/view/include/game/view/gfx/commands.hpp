#pragma once

#include "oge/color.hpp"
#include "oge/rect.hpp"
#include "oge/point3.hpp"
#include "oge/runtime/gfx/commands.hpp"

namespace game::math
{
    using namespace oge::math;
}

namespace game::view::gfx
{
using oge::runtime::GPUChunkedAllocation;
using oge::Point3;

using namespace oge::rects;
using namespace oge::colors;
using namespace oge::gpu_objects;

struct PDebugRect : IRect
{
    ColorRGBA8 color = COLOR_WHITE;
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
    GPUBufferSpan vertices;
    GPUBufferSpan indices;
    uint32_t indexCount;
};

struct CmdDrawGeneralMeshOpaque
{
    GPUBufferHandle vertexBuffer;
    GPUBufferHandle indexBuffer;
    uint32_t indexCount;
};

struct CmdDrawTerrainMeshOpaque
{
    PTerrainMesh terrainMesh;
    Point3 coords;
};

struct CmdDrawDebugText
{
    std::string text;
    ColorRGBA8 color;
};

struct CmdDrawDebugRect
{
    IRect16 rect;
    ColorRGBA8 color;
};

struct CmdAddView
{
    IRect16 rect;
    math::mat4 view = math::lookAt(math::vec3(20, 20, 20), math::vec3(0, 0, 0), math::vec3(0, 1, 0));
    float fov = math::radians(45.0f);
    float aspect = 0.f;
};

class RequiresVPTransform
{
};

} // namespace game::view::gfx
