#pragma once

#include "oge/runtime/gfx/commands.hpp"
#include "oge/runtime/gfx/draw_context.hpp"
#include "oge/submission_group.hpp"

namespace oge::runtime::gfx
{
class UIPass : Pass<CmdDrawSprite>
{
   public:
    struct PushConstant
    {
        math::mat2 transform;
        math::vec2 offset;
    };

    struct Vertex
    {
        U16Point2 position;  // 4 byte
        U16Norm2 uv;         // 4 byte
        ColorRGBA8 color;    // 4 byte
    };

    void onAttach(InitDrawContext& ctx);
    void onDetach(InitDrawContext& ctx);
    void onUpdate(DrawContext& ctx, View view);

   private:
    GPUBindingGroupHandle GetOrCreateBindingGroup(IGraphicsBackend& backend, GPUTextureHandle texture);

    std::unordered_map<GPUTextureHandle, std::vector<Vertex>, HandleHash<GPUTextureHandle>> classedVertices;
    std::vector<uint16_t> indices;

    std::unordered_map<GPUTextureHandle, GPUBindingGroupHandle, HandleHash<GPUTextureHandle>> cachedBindingGroups;

    PushConstant pushConstant;

    GPUBufferHandle vertexBuffer;
    void* vertexBufferCpu;
    GPUBufferHandle indexBuffer;
    GPUPipelineHandle pipelineHandle;
    GPUBindingGroupLayoutHandle bindingGroupLayout;
};
}  // namespace oge::runtime::renderer