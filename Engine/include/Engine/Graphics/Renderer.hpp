#pragma once

#include "DebugPass.hpp"
#include "Engine/ClassHelper.hpp"
#include "Engine/Math.hpp"
#include "Engine/entt.hpp"
#include "RingStagingBuffer.hpp"
#include "TerrainPass.hpp"
#include "UIPass.hpp"
#include "UniformArena.hpp"

namespace OneGame::Engine
{
class AssetManager;
class StreamingManager;
class AssetPool;
}  // namespace OneGame::Engine

namespace OneGame::Engine::Graphics
{
class IGraphicsBackend;

class Renderer
{
   public:
    NO_COPY(Renderer)
    void Initialize(IGraphicsBackend& backend, AssetPool& pool);
    void Shutdown(IGraphicsBackend& backend);
    void Prepare(IGraphicsBackend& backend, entt::registry& world, float deltaTime);
    void Render(IGraphicsBackend& backend, float deltaTime);

    void EnableTerrainPass(IGraphicsBackend& backend, AssetPool& bundle, Mesh terrainMesh);
    void DisableTerrainPass(IGraphicsBackend& backend);
    void EnableTerrainPass2(IGraphicsBackend& backend, AssetPool& bundle, GPUBufferHandle storageBuf);
    void DisableTerrainPass2(IGraphicsBackend& backend);

   private:
    void Draw(DrawContext& ctx);

    std::function<void(Renderer*, DrawContext&)> currentDraw;

    UniformArena uniformArena;
    UIPass uiPass;
    TestPass testPass;
    DebugInfoPass debugInfoPass;
    // TerrainPass terrainPass;
    TerrainPass2 terrainPass2;

    bool enableTerrainPass = false;
    bool enableTerrainPass2 = false;
};
}  // namespace OneGame::Engine::Graphics

namespace OneGame::Engine
{
using Renderer = Graphics::Renderer;
}
