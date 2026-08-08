#pragma once

#include "oge/graphics/configs.hpp"
#include "oge/runtime/gfx/pass.hpp"
#include "oge/runtime/gfx/draw_context.hpp"
#include "oge/runtime/gfx/uniform_arena.hpp"
#include "oge/submission_group.hpp"
#include "game/view/gfx/commands.hpp"
#include "oge/runtime/objects_ext.hpp"
#include "oge/point2.hpp"
#include "oge/graphics/backend.hpp"

namespace game::view::gfx
{
using oge::U16Point2;
using oge::U16Norm2;
using oge::runtime::gfx::Pass;
using oge::runtime::InitDrawContext;
using oge::runtime::DrawContext;
using oge::graphics::IGraphicsBackend;
using oge::graphics::BufferUsage;
using oge::runtime::FrameArena;

class UIPass : public Pass<oge::runtime::CmdDrawSprite>, public RequiresScreenAffine
{
   public:
    using PushConstant = ScreenAffine;

    struct Vertex
    {
        U16Point2 position;  // 4 byte
        U16Norm2 uv;         // 4 byte
        ColorRGBA8 color;    // 4 byte
    };

    void onAttach(InitDrawContext& ctx);
    void onDetach(InitDrawContext& ctx);
    void onUpdate(DrawContext& ctx, View view, ScreenAffine pushConstant);

   private:
    GPUBindingGroupHandle GetOrCreateBindingGroup(IGraphicsBackend& backend,
                                                  GPUTextureHandle texture);

    std::unordered_map<GPUTextureHandle, std::vector<Vertex>>
        classedVertices;
    std::vector<uint16_t> indices;

    std::unordered_map<GPUTextureHandle, GPUBindingGroupHandle>
        cachedBindingGroups;

    FrameArena vertexArena = {BufferUsage::Vertex};
    FrameArena indexArena = {BufferUsage::Index};
    GPUPipelineHandle pipelineHandle;
    GPUBindingGroupLayoutHandle bindingGroupLayout;
};
}  // namespace oge::runtime::gfx
