#include "Engine/Graphics/TerrainPass.hpp"

#include "Engine/Graphics/IGraphicsBackend.hpp"
#include "Engine/Terrain/TerrainVertexFormat.hpp"
#include "Engine/entt.hpp"
#include "RendererInternals.hpp"
#define LOGGER_NAME "Engine"
#include "Engine/Logger.hpp"
#include "Engine/GameAppState.hpp"
#include "Engine/StreamingManager.hpp"
#include "Engine/AssetManager.hpp"

namespace OneGame::Engine::Graphics
{
using namespace Terrain;

void TerrainPass2::Enable(IGraphicsBackend& backend, InitContext& ctxt)
{
    BindingGroupLayoutDesc layout{};
    layout.textureCount = 1;
    layout.bufferCount = 2;
    layout.dynamicBufferMask = 0b11;
    layout.storageBufferMask = 0b11;
    bindingGroupLayout = backend.CreateBindingGroupLayout(layout);

    blockTexture = ctxt.assets.backend.AllocateGPUTexture(BLOCK_TEXTURE_SIZE, BLOCK_TEXTURE_SIZE, 256);
    auto invalidBlockTextureBlob = ctxt.assets.assetManager.LoadTexture("invalid.png");
    for (int i = 0; i < 256; ++i)
    {
        ctxt.assets.streamingManager.UploadTexture<UploadType::Immediate>(invalidBlockTextureBlob->data, blockTexture, i);
    }
    
    // auto tex = ctxt.assets.assetManager.LoadTexture("invalid.png");
    // auto& info = tex->info;
    // blockTexture = backend.AllocateGPUTexture(info.width, info.height);
    // ctxt.assets.streamingManager.UploadTexture<UploadType::Immediate>(tex->data, blockTexture, 0);

    {
        GraphicsPipelineDesc desc{};
        ctxt.assets.LoadBlob("terrain2.vert.opt.spv", desc.vertexShader);
        ctxt.assets.LoadBlob("terrain2.frag.opt.spv", desc.fragmentShader);

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
    assets.streamingManager.UploadTexture<UploadType::Immediate>(assets.assetManager.LoadTexture(id)->data, blockTexture, slot);
}

GPUBindingGroupHandle TerrainPass2::GetOrCreateBindingGroup(IGraphicsBackend& backend, UniformArena& arena, GPUBufferHandle storageBuffer, uint32_t chunkSize)
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

void TerrainPass2::Disable(IGraphicsBackend& backend)
{
    backend.DestroyPipeline(pipelineHandle);
    for (auto [buf, bg] : cachedBindingGroups)
    {
        backend.DestroyBindingGroup(bg);
    }
    backend.DestroyBindingGroupLayout(bindingGroupLayout);
    backend.DestroyTexture(blockTexture);
}

void TerrainPass2::Draw(DrawContext& ctxt)
{
    activeChunkSlots.clear();
    for (auto& cmd : ctxt.world.Get<CmdDrawTerrainMeshOpaque>())
    {
        auto& mesh = cmd.terrainMesh;
        auto& coord = cmd.coords;
        auto resolved = ctxt.chunkAllocator.Resolve(mesh.alloc);
        auto group = GetOrCreateBindingGroup(ctxt.backend, ctxt.uniformArena, resolved.buffer, resolved.size);
        auto it = activeChunkSlots.find(group);
        if (it == activeChunkSlots.end())
        {
            std::vector<TerrainMesh> v;
            auto [newIt, _] = activeChunkSlots.emplace(group, std::move(v));
            it = newIt;
        }
        it->second.emplace_back(resolved.offset, mesh.indexCount, coord);
    }

    ctxt.drawCmd.BindGraphicsPipeline(pipelineHandle);
    for (auto [bindingGroup, val] : activeChunkSlots)
    {
        ubos.clear();
        for (auto mesh : val)
        {
            auto coord = mesh.coord;
            auto model = math::translate(
                math::mat4(1.0f),
                math::vec3(coord.x * CHUNK_SIZE_X, coord.y * CHUNK_SIZE_Y, coord.z * CHUNK_SIZE_Z));
            UBO ubo{ctxt.pvTransform * model};
            ubos.push_back(ubo);
        }
        if (ubos.size() == 0) continue;

        auto uniformMemory = ctxt.uniformArena.Allocate(sizeof(UBO) * ubos.size());
        std::memcpy(uniformMemory.cpuPtr, ubos.data(), sizeof(UBO) * ubos.size());

        for (uint32_t i = 0; i < ubos.size(); ++i)
        {
            uint32_t offset[2] = {val[i].offset,
                                static_cast<uint32_t>(uniformMemory.offset + i * sizeof(UBO))};
            ctxt.drawCmd.BindBindingGroup(bindingGroup, 0, offset);
            ctxt.drawCmd.Draw(val[i].indexCount);
        }
    }
}
}  // namespace OneGame::Engine::Graphics
