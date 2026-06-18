#pragma once

#include <entt/entt.hpp>

#include "UniformArena.hpp"
#include "RingStagingBuffer.hpp"
#include "IGraphicsBackend.hpp"
#include "UIPass.hpp"
#include "DebugPass.hpp"
#include "TerrainPass.hpp"

#include "Engine/Math.hpp"


namespace OneGame::Engine::Graphics
{
    class Renderer
    {
    public:
        virtual ~Renderer() = default;
        void Initialize(IGraphicsBackend* backend, AssetManager& assets, StreamingManager& streaming);
        void Shutdown(IGraphicsBackend* backend);
        void Prepare(entt::registry* world, const IGraphicsBackend* backend);
        void Render(IGraphicsBackend* backend, float deltaTime);

    private:
        UniformArena uniformArena;
        UIPass uiPass;
        TestPass testPass;
        DebugInfoPass debugInfoPass;
        TerrainPass terrainPass;

        bool isFirstFrame = true;
    };
}
