#include "game/view/gfx/gizmo_pass.hpp"

#include <cstddef>
#include <cstdint>

#include "game/view/gfx/commands.hpp"
#include "internals.hpp"
#include "oge/log.hpp"

namespace game::view::gfx
{
void GizmoPass::onAttach(InitDrawContext& ctx)
{
    auto& backend = ctx.assets.backend;
    LOG_DEBUG("begin initialize gizmo pass");

    BindingGroupLayoutDesc layout{};
    layout.textureCount = 0;
    layout.bufferCount = 0;
    layout.dynamicBufferMask = 0;
    m_bindingLayout = backend.CreateBindingGroupLayout(layout);
    LOG_DEBUG("gizmo binding group layout created");

    {
        GraphicsPipelineDesc desc{};
        if (!ctx.assets.LoadBlob("gizmo.vert.opt.spv", desc.vertexShader))
            throw std::runtime_error("failed to load gizmo vertex shader");
        if (!ctx.assets.LoadBlob("gizmo.frag.opt.spv", desc.fragmentShader))
            throw std::runtime_error("failed to load gizmo fragment shader");

        // position (vec3)
        desc.vertexLayout.push_back(VertexAttributeFormat::Float32x3);
        // color (RGBA8 unorm -> vec4)
        desc.vertexLayout.push_back(VertexAttributeFormat::UniformUint8x4);

        desc.bindingGroupLayouts.push_back(m_bindingLayout);
        desc.writeDepth = true;
        desc.blending = true;
        desc.cullMode = CullMode::None;
        desc.topology = PrimitiveTopology::LineList;

        PushConstantRangeDesc cDesc{};
        cDesc.offset = 0;
        cDesc.size = sizeof(PushConstant);
        cDesc.stageFlags = ShaderStage::Vertex;
        desc.pushConstants.push_back(cDesc);

        m_pipelineHandle = backend.CreateGraphicsPipeline(desc);
        LOG_DEBUG("gizmo pipeline created");
    }

    m_vertexArena.Initialize(backend, MAX_GIZMO_VERTICES * sizeof(GizmoVertex));
    LOG_DEBUG("gizmo vertex buffer created");
}

void GizmoPass::onDetach(InitDrawContext& ctx)
{
    auto& backend = ctx.assets.backend;
    m_vertexArena.Shutdown(backend);
    backend.DestroyBindingGroupLayout(m_bindingLayout);
    backend.DestroyPipeline(m_pipelineHandle);
}

void GizmoPass::onUpdate(DrawContext& ctx, View view,
                         const math::mat4& pvTransform)
{
    m_vertices.clear();

    // Collect wire cube commands
    for (auto& cmd : view.Get<CmdDrawWireCube>())
    {
        EmitWireCube(cmd, m_vertices);
    }

    // Collect wire rect commands
    for (auto& cmd : view.Get<CmdDrawWireRect>())
    {
        EmitWireRect(cmd, m_vertices);
    }

    if (m_vertices.empty()) return;

    // Upload vertices
    size_t byteSize = m_vertices.size() * sizeof(GizmoVertex);
    auto vAlloc = m_vertexArena.Allocate(byteSize);
    std::memcpy(vAlloc.cpuPtr, m_vertices.data(), byteSize);
    m_vertexArena.AdvanceFrame();
    m_vertexArena.Flush(ctx.backend);

    auto& cmd = ctx.drawCmd;
    cmd.BindGraphicsPipeline(m_pipelineHandle);
    cmd.PushConstants(ShaderStage::Vertex, &pvTransform, sizeof(PushConstant));
    cmd.BindVertexBuffer(m_vertexArena.GetBuffer(), vAlloc.offset);
    cmd.Draw(static_cast<uint32_t>(m_vertices.size()), 1, 0, 0);
}

void GizmoPass::EmitWireCube(const CmdDrawWireCube& cube,
                             std::vector<GizmoVertex>& out)
{
    const float e = cube.extent;
    const Point3& c = cube.center;
    const ColorRGBA8 col = cube.color;

    // 8 corners of the cube
    math::vec3 corners[8] = {
        math::vec3(c.x - e, c.y - e, c.z - e),  // 0
        math::vec3(c.x + e, c.y - e, c.z - e),  // 1
        math::vec3(c.x + e, c.y + e, c.z - e),  // 2
        math::vec3(c.x - e, c.y + e, c.z - e),  // 3
        math::vec3(c.x - e, c.y - e, c.z + e),  // 4
        math::vec3(c.x + e, c.y - e, c.z + e),  // 5
        math::vec3(c.x + e, c.y + e, c.z + e),  // 6
        math::vec3(c.x - e, c.y + e, c.z + e),  // 7
    };

    // 12 lines (24 vertices) for the wireframe cube
    auto addLine = [&](int a, int b)
    {
        out.push_back({corners[a], col});
        out.push_back({corners[b], col});
    };

    // Bottom face
    addLine(0, 1);
    addLine(1, 2);
    addLine(2, 3);
    addLine(3, 0);
    // Top face
    addLine(4, 5);
    addLine(5, 6);
    addLine(6, 7);
    addLine(7, 4);
    // Vertical edges
    addLine(0, 4);
    addLine(1, 5);
    addLine(2, 6);
    addLine(3, 7);
}

void GizmoPass::EmitWireRect(const CmdDrawWireRect& rect,
                             std::vector<GizmoVertex>& out)
{
    const auto& col = rect.color;

    // 4 corners of the rectangle: center +/- uAxis*uExtent +/- vAxis*vExtent
    math::vec3 u = rect.uAxis * rect.uExtent;
    math::vec3 v = rect.vAxis * rect.vExtent;
    math::vec3 c = math::vec3(rect.center.x, rect.center.y, rect.center.z);

    math::vec3 p00 = c - u - v;
    math::vec3 p10 = c + u - v;
    math::vec3 p11 = c + u + v;
    math::vec3 p01 = c - u + v;

    // 4 lines (8 vertices)
    out.push_back({p00, col});
    out.push_back({p10, col});
    out.push_back({p10, col});
    out.push_back({p11, col});
    out.push_back({p11, col});
    out.push_back({p01, col});
    out.push_back({p01, col});
    out.push_back({p00, col});
}

}  // namespace game::view::gfx
