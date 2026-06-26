#pragma once

#include <entt/entt.hpp>

#include "UniformArena.hpp"
#include "RingStagingBuffer.hpp"
#include "UIPass.hpp"
#include "DebugPass.hpp"
#include "TerrainPass.hpp"

#include "Engine/Math.hpp"
#include "Engine/ClassHelper.hpp"

namespace OneGame::Engine
{
    class AssetManager;
    class StreamingManager;
    class AssetBundleWriter;
}

namespace OneGame::Engine::Graphics
{
    class IGraphicsBackend;

    class Renderer
    {
    public:
        NO_COPY(Renderer)
        void Initialize(IGraphicsBackend& backend, AssetManager& am, StreamingManager& sm);
        void Shutdown(IGraphicsBackend& backend);
        void Prepare(IGraphicsBackend& backend, entt::registry& world, float deltaTime);
        void Render(IGraphicsBackend& backend, float deltaTime);

        void EnableTerrainPass(IGraphicsBackend& backend, AssetBundleWriter& bundle, Mesh terrainMesh);
        void DisableTerrainPass(IGraphicsBackend& backend);
        void EnableTerrainPass2(IGraphicsBackend& backend, AssetBundleWriter& bundle, GPUBufferHandle storageBuf);
        void DisableTerrainPass2(IGraphicsBackend& backend);

    private:
        void Draw(DrawContext& ctx);

        std::function<void(Renderer*, DrawContext&)> currentDraw;

        UniformArena uniformArena;
        UIPass uiPass;
        TestPass testPass;
        DebugInfoPass debugInfoPass;
        //TerrainPass terrainPass;
        TerrainPass2 terrainPass2;

        bool enableTerrainPass = false;
        bool enableTerrainPass2 = false;
    };
}

namespace OneGame::Engine
{
    using Renderer = Graphics::Renderer;
}
