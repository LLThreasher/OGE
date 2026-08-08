#pragma once

#include "game/view/gfx/commands.hpp"
#include "game/view/submission_queue.hpp"
#include "oge/point3.hpp"
#include "oge/runtime/gfx/draw_context.hpp"
#include "oge/runtime/gfx/pass.hpp"

namespace game::view::gfx
{

constexpr uint32_t BLOCK_TEXTURE_SIZE = 16;

class TerrainPass2 : public RequiresVPTransform,
                     public oge::runtime::gfx::Pass<CmdDrawTerrainMeshOpaque>
{
    struct TerrainMesh
    {
        uint32_t offset;
        uint32_t indexCount;
        Point3 coord;
    };

    struct UBO
    {
        math::mat4 mvp;
    };

   public:
    TerrainPass2()
    {
    }

    void UpdateBlockTexture(AssetContext& assets, const std::string& id,
                            uint32_t slot);
    void onAttach(InitDrawContext& ctx);
    void onDetach(InitDrawContext& ctx);
    void onUpdate(DrawContext& ctx, View view, const math::mat4& pvTransform);

   private:
    GPUBindingGroupHandle GetOrCreateBindingGroup(IGraphicsBackend& backend,
                                                  FrameArena& arena,
                                                  GPUBufferHandle storageBuffer,
                                                  uint32_t chunkSize);

    std::unordered_map<GPUBindingGroupHandle, std::vector<TerrainMesh>>
        activeChunkSlots;
    std::vector<UBO> ubos;

    std::unordered_map<GPUBufferHandle, GPUBindingGroupHandle>
        cachedBindingGroups;

    GPUPipelineHandle pipelineHandle;
    GPUBindingGroupLayoutHandle bindingGroupLayout;
    GPUTextureHandle blockTexture;
};
}  // namespace game::view::gfx