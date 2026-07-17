#pragma once

#include "oge/color.hpp"
#include "oge/log.hpp"
#include "oge/runtime/renderer/context.hpp"
#include "oge/runtime/submission_queue.hpp"

namespace oge::runtime::renderer
{
using namespace graphics;

constexpr size_t NUM_DEBUG_VERTICES = 2048;
constexpr size_t NUM_DEBUG_INDICES = NUM_DEBUG_VERTICES / 4 * 6;

class DebugInfoPass
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
    using View = SubmissionView<CmdDrawDebugText, CmdDrawDebugRect>;
    void onAttach(InitDrawContext& ctx);
    void onDetach(InitDrawContext& ctx);
    void onUpdate(DrawContext& ctx, View view);

   private:
    Vertex vertices[NUM_DEBUG_VERTICES];
    uint16_t indices[NUM_DEBUG_INDICES];
    size_t numQuads;
    PushConstant pushConstant;

    GPUPipelineHandle pipeline;
    GPUBindingGroupLayoutHandle bindingGroupLayout;
    GPUBufferHandle vertexBuffer;
    GPUBufferHandle indexBuffer;
    GPUBindingGroupHandle bindingGroup;
};
}  // namespace oge::runtime::renderer