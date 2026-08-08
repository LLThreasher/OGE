#pragma once

#include "oge/color.hpp"
#include "oge/point3.hpp"
#include "oge/rect.hpp"
#include "oge/math.hpp"
#include "oge/runtime/objects_ext.hpp"

namespace game
{
namespace view::gfx
{
namespace math = ::oge::math;
using oge::Point3;
using oge::runtime::GPUChunkedAllocation;
using oge::runtime::PSprite;

using namespace oge::rects;
using namespace oge::colors;
using namespace oge::graphics::gpu_objects;

struct PDebugRect : IRect
{
    ColorRGBA8 color = WHITE;
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
    math::mat4 view = math::lookAt(math::vec3(20, 20, 20), math::vec3(0, 0, 0),
                                   math::vec3(0, 1, 0));
    float fov = math::radians(45.0f);
    float aspect = 0.f;
};

class RequiresVPTransform
{
};

class RequiresScreenAffine
{
};

struct ScreenAffine
{
    math::mat2 transform;
    math::vec2 offset;
};

}  // namespace game::view::gfx
}
