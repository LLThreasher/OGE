#pragma once

#include "DebugPass.hpp"
#include "Engine/ClassHelper.hpp"
#include "Engine/Math.hpp"
#include "Engine/entt.hpp"
#include "RingStagingBuffer.hpp"
#include "TerrainPass.hpp"
#include "UIPass.hpp"
#include "UniformArena.hpp"
#include "ChunkAllocator2.hpp"

namespace OneGame::Engine
{
class AssetManager;
class StreamingManager;
class AssetPool;
class AssetContext;
}  // namespace OneGame::Engine

namespace OneGame::Engine::Graphics
{
class IGraphicsBackend;

class Renderer
{
   public:
    NO_COPY(Renderer)
    void Initialize(AssetContext& assets);
    void Shutdown(AssetContext& assets);
    void Render(AssetContext& assets, const entt::registry& world, float deltaTime);

    void EnableTerrainPass(AssetContext& assets, Mesh terrainMesh);
    void DisableTerrainPass(AssetContext& assets);
    void EnableTerrainPass2(AssetContext& assets, GPUBufferHandle storageBuf);
    void DisableTerrainPass2(AssetContext& assets);

    GPUChunkedAllocation AllocateTerrainMesh(uint32_t size)
    {
        return chunkAllocator.Allocate(size);
    }
    GPUBufferRange ResolveTerrainMesh(IGraphicsBackend& backend, GPUChunkedAllocation alloc)
    {
        chunkAllocator.CreateBuffers(backend);
        return chunkAllocator.Resolve(alloc);
    }
    void FreeTerrainMesh(GPUChunkedAllocation alloc)
    {
        chunkAllocator.Free(alloc);
    }

   private:
    void Draw(DrawContext& ctx);
    void RenderView(AssetContext& assets, DrawContext drawCtxt);

    std::function<void(Renderer*, DrawContext&)> currentDraw;

    UniformArena uniformArena;
    DynamicChunkAllocator chunkAllocator;

    UIPass uiPass;
    TestPass testPass;
    DebugInfoPass debugInfoPass;
    TerrainPass2 terrainPass2;

    bool enableTerrainPass2 = false;
};
}  // namespace OneGame::Engine::Graphics

namespace OneGame::Engine
{
using Renderer = Graphics::Renderer;
}
