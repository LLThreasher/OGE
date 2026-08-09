#pragma once

#include <vector>

#include "game/view/gfx/commands.hpp"
#include "game/view/gfx/gizmo_commands.hpp"
#include "oge/graphics/backend.hpp"
#include "oge/graphics/configs.hpp"
#include "oge/math.hpp"
#include "oge/runtime/gfx/draw_context.hpp"
#include "oge/runtime/gfx/pass.hpp"
#include "oge/runtime/gfx/uniform_arena.hpp"

namespace game::view::gfx
{

using oge::runtime::gfx::Pass;
using oge::runtime::InitDrawContext;
using oge::runtime::DrawContext;
using oge::graphics::IGraphicsBackend;
using oge::graphics::BufferUsage;
using oge::runtime::FrameArena;
namespace math = ::oge::math;

// =========================================================================
// GizmoPass
//
// Renders wireframe gizmos (CmdDrawWireCube, CmdDrawWireRect) in 3D space
// using the gizmo.{vert,frag} shaders.
//
// Wireframe rendering requires:
//   - pipeline created with VK_POLYGON_MODE_LINE
//   - depth test enabled, depth write enabled
// =========================================================================

class GizmoPass : public RequiresVPTransform,
                  public Pass<CmdDrawWireCube, CmdDrawWireRect>
{
   public:
    static constexpr size_t MAX_GIZMO_VERTICES = 65536;

    using PushConstant = math::mat4;  // view-projection matrix

    struct GizmoVertex
    {
        math::vec3 position;
        ColorRGBA8 color;
    };

    void onAttach(InitDrawContext& ctx);
    void onDetach(InitDrawContext& ctx);
    void onUpdate(DrawContext& ctx, View view, const math::mat4& pvTransform);

   private:
    // Generate wireframe cube vertices (12 lines = 24 vertices).
    void EmitWireCube(const CmdDrawWireCube& cmd, std::vector<GizmoVertex>& out);
    // Generate wireframe rect vertices (4 lines = 8 vertices).
    void EmitWireRect(const CmdDrawWireRect& cmd, std::vector<GizmoVertex>& out);

    std::vector<GizmoVertex> m_vertices;
    FrameArena m_vertexArena{BufferUsage::Vertex};
    GPUPipelineHandle m_pipelineHandle;
    GPUBindingGroupLayoutHandle m_bindingLayout;
};

}  // namespace game::view::gfx
