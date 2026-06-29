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
    void Prepare(AssetContext& assets, entt::registry& world, float deltaTime);
    void Render(IGraphicsBackend& backend, float deltaTime);

    void EnableTerrainPass(AssetContext& assets, Mesh terrainMesh);
    void DisableTerrainPass(AssetContext& assets);
    void EnableTerrainPass2(AssetContext& assets, GPUBufferHandle storageBuf);
    void DisableTerrainPass2(AssetContext& assets);

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
