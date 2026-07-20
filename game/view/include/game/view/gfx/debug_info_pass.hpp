#pragma once

#include "oge/graphics/configs.hpp"
#include "oge/runtime/gfx/draw_context.hpp"
#include "oge/color.hpp"
#include "oge/log.hpp"
#include "game/view/submission_queue.hpp"
#include "oge/runtime/gfx/uniform_arena.hpp"

namespace game::view::gfx
{
using namespace oge::graphics;

constexpr uint32_t NUM_DEBUG_VERTICES = 2048;
constexpr uint32_t NUM_DEBUG_INDICES = NUM_DEBUG_VERTICES / 4 * 6;

class DebugInfoPass : public Pass<CmdDrawDebugText, CmdDrawDebugRect>
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
    void onUpdate(DrawContext& ctx, View view);

   private:
    size_t numQuads;
    PushConstant pushConstant;

    GPUPipelineHandle pipeline;
    GPUBindingGroupLayoutHandle bindingGroupLayout;
    GPUBindingGroupHandle bindingGroup;
    
    FrameArena vertexArena = {BufferUsage::Vertex};
    FrameArena indexArena = {BufferUsage::Index};
};
}  // namespace game::view::gfx