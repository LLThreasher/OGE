#pragma once

#include "oge/aabb.hpp"
#include "oge/color.hpp"
#include "oge/math.hpp"
#include "oge/point3.hpp"
#include "oge/runtime/objects_ext.hpp"

namespace game::view::gfx
{

namespace math = ::oge::math;
using oge::AABB;
using oge::Point3;
using oge::colors::ColorRGBA8;
using oge::colors::RED;
using oge::colors::GREEN;
using oge::colors::BLUE;
using namespace oge::graphics::gpu_objects;

// =========================================================================
// Gizmo commands
//
// Gizmos are debug-only wireframe primitives drawn in 3D space.
// They use the GizmoPass which renders with the gizmo shader.
// =========================================================================

/// Axis-aligned wireframe cube defined by an AABB (min/max).
/// Drawn with 12 lines (24 vertices).
struct CmdDrawWireCube
{
    AABB aabb;
    ColorRGBA8 color = RED;
};

/// Wireframe rectangle in 3D space.  Defined by a center point, two
/// orthogonal axis vectors, and half-extents along each axis.
struct CmdDrawWireRect
{
    Point3 center;
    math::vec3 uAxis = math::vec3(1, 0, 0);  // first axis direction
    math::vec3 vAxis = math::vec3(0, 1, 0);  // second axis direction
    float uExtent = 1.0f;                     // half-size along uAxis
    float vExtent = 1.0f;                     // half-size along vAxis
    ColorRGBA8 color = GREEN;
};

}  // namespace game::view::gfx
