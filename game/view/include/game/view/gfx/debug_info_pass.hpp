#pragma once

#include "game/view/gfx/commands.hpp"
#include "game/view/submission_queue.hpp"
#include "oge/runtime/gfx/pass.hpp"
#include "oge/color.hpp"
#include "oge/graphics/configs.hpp"
#include "oge/log.hpp"
#include "oge/runtime/gfx/draw_context.hpp"
#include "oge/runtime/gfx/uniform_arena.hpp"

namespace game::view::gfx
{
using namespace oge::graphics;
using oge::runtime::gfx::Pass;

constexpr uint32_t NUM_DEBUG_VERTICES = 2048;
constexpr uint32_t NUM_DEBUG_INDICES = NUM_DEBUG_VERTICES / 4 * 6;

class DebugInfoPass : public Pass<CmdDrawDebugText, CmdDrawDebugRect>, public RequiresScreenAffine
{
    struct PushConstant
    {
        math::mat2 transform;
        math::vec2 offset;
    };

    struct Vertex
    {
        math::vec3 pos;
        ColorRGBA8 color;
    };

   public:
    void onAttach(InitDrawContext& ctx);
    void onDetach(InitDrawContext& ctx);
    void onUpdate(DrawContext& ctx, View view, ScreenAffine pushConstants);

   private:
    size_t numQuads;

    GPUPipelineHandle pipeline;
    GPUBindingGroupLayoutHandle bindingGroupLayout;
    GPUBindingGroupHandle bindingGroup;

    FrameArena vertexArena = {BufferUsage::Vertex};
    FrameArena indexArena = {BufferUsage::Index};
};
}  // namespace game::view::gfx