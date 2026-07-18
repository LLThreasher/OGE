#include "game/view/gfx/terrain_pass.hpp"

#include "internals.hpp"
#include "game/terrain/defs.hpp"
#include "oge/runtime/gfx/chunk_allocator2.hpp"

namespace game::view
{
using namespace ::oge::graphics;
using namespace game::terrain;
void TerrainPass2::onAttach(InitDrawContext& ctx)
{
    auto& backend = ctx.assets.backend;
    BindingGroupLayoutDesc layout{};
    layout.textureCount = 1;
    layout.bufferCount = 2;
    layout.dynamicBufferMask = 0b11;
    layout.storageBufferMask = 0b11;
    bindingGroupLayout = backend.CreateBindingGroupLayout(layout);

    blockTexture = ctx.assets.backend.AllocateGPUTexture(BLOCK_TEXTURE_SIZE, BLOCK_TEXTURE_SIZE, 256);
    auto invalidBlockTextureBlob = ctx.assets.assetManager.LoadTexture("invalid.png");
    for (uint32_t i = 0; i < 256; ++i)
    {
        ctx.assets.streamingManager.UploadTexture<UploadType::Immediate>(
            invalidBlockTextureBlob->data, TextureTarget{.texture = blockTexture,
                                                         .region = {0, 0, BLOCK_TEXTURE_SIZE, BLOCK_TEXTURE_SIZE},
                                                         .baseTextureLayer = i});
    }

    // auto tex = ctx.assets.assetManager.LoadTexture("invalid.png");
    // auto& info = tex->info;
    // blockTexture = backend.AllocateGPUTexture(info.width, info.height);
    // ctx.assets.streamingManager.UploadTexture<UploadType::Immediate>(tex->data, blockTexture, 0);

    {
        GraphicsPipelineDesc desc{};
        ctx.assets.LoadBlob("terrain2.vert.opt.spv", desc.vertexShader);
        ctx.assets.LoadBlob("terrain2.frag.opt.spv", desc.fragmentShader);

        desc.bindingGroupLayouts.push_back(bindingGroupLayout);
        desc.writeDepth = true;
        desc.blending = false;
        desc.depthTest = true;
        desc.depthCompareOp = DepthCompareOp::GreaterEqual;
        desc.cullMode = CullMode::Back;
        pipelineHandle = backend.CreateGraphicsPipeline(desc);
    }
}

void TerrainPass2::UpdateBlockTexture(AssetContext& assets, const std::string& id, uint32_t slot)
{
    assets.streamingManager.UploadTexture<UploadType::Immediate>(
        assets.assetManager.LoadTexture(id)->data,
        TextureTarget{blockTexture, {0, 0, BLOCK_TEXTURE_SIZE, BLOCK_TEXTURE_SIZE}, slot});
}

GPUBindingGroupHandle TerrainPass2::GetOrCreateBindingGroup(IGraphicsBackend& backend, UniformArena& arena,
                                                            GPUBufferHandle storageBuffer, uint32_t chunkSize)
{
    auto it = cachedBindingGroups.find(storageBuffer);
    if (it != cachedBindingGroups.end())
    {
        return it->second;
    }
    else
    {
        BindingGroupDesc desc{};
        desc.layout = bindingGroupLayout;
        desc.textures.push_back(blockTexture);
        desc.buffers.push_back({storageBuffer, chunkSize});
        desc.buffers.push_back({arena.GetBuffer(), sizeof(UBO)});
        auto [newIt, inserted] = cachedBindingGroups.emplace(storageBuffer, backend.CreateBindingGroup(desc));
        return newIt->second;
    }
}

void TerrainPass2::onDetach(InitDrawContext& ctx)
{
    auto& backend = ctx.assets.backend;
    backend.DestroyPipeline(pipelineHandle);
    for (auto [buf, bg] : cachedBindingGroups)
    {
        backend.DestroyBindingGroup(bg);
    }
    backend.DestroyBindingGroupLayout(bindingGroupLayout);
    backend.DestroyTexture(blockTexture);
}

void TerrainPass2::onUpdate(DrawContext& ctx, View view, const math::mat4& pvTransform)
{
    activeChunkSlots.clear();
    for (auto& cmd : view.Get<CmdDrawTerrainMeshOpaque>())
    {
        auto& mesh = cmd.terrainMesh;
        auto& coord = cmd.coords;
        auto resolved = ctx.chunkAllocator.Resolve(mesh.alloc);
        auto group = GetOrCreateBindingGroup(ctx.backend, ctx.uniformArena, resolved.buffer, resolved.size);
        auto it = activeChunkSlots.find(group);
        if (it == activeChunkSlots.end())
        {
            std::vector<TerrainMesh> v;
            auto [newIt, _] = activeChunkSlots.emplace(group, std::move(v));
            it = newIt;
        }
        it->second.emplace_back(resolved.offset, mesh.indexCount, coord);
    }

    ctx.drawCmd.BindGraphicsPipeline(pipelineHandle);
    for (auto [bindingGroup, val] : activeChunkSlots)
    {
        ubos.clear();
        for (auto mesh : val)
        {
            auto coord = mesh.coord;
            auto model = math::translate(
                math::mat4(1.0f), math::vec3(coord.x * CHUNK_SIZE_X, coord.y * CHUNK_SIZE_Y, coord.z * CHUNK_SIZE_Z));
            UBO ubo{pvTransform * model};
            ubos.push_back(ubo);
        }
        if (ubos.size() == 0) continue;

        auto uniformMemory = ctx.uniformArena.Allocate(sizeof(UBO) * ubos.size());
        std::memcpy(uniformMemory.cpuPtr, ubos.data(), sizeof(UBO) * ubos.size());

        for (uint32_t i = 0; i < ubos.size(); ++i)
        {
            uint32_t offset[2] = {val[i].offset, static_cast<uint32_t>(uniformMemory.offset + i * sizeof(UBO))};
            ctx.drawCmd.BindBindingGroup(bindingGroup, 0, offset);
            ctx.drawCmd.Draw(val[i].indexCount);
        }
    }
}
}  // namespace oge::runtime::renderer
