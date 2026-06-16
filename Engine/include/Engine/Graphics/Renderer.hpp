#pragma once

#include <entt/entt.hpp>

#include "UniformArena.hpp"
#include "RingStagingBuffer.hpp"
#include "IGraphicsBackend.hpp"
#include "UIPass.hpp"
#include "DebugPass.hpp"

#include "Engine/Math.hpp"


namespace OneGame::Engine::Graphics
{
    class Renderer
    {
    public:
        virtual ~Renderer() = default;
        void Initialize(IGraphicsBackend* backend, AssetManager& assets);
        void Shutdown(IGraphicsBackend* backend);
        void Prepare(entt::registry* world);
        void Render(IGraphicsBackend* backend, float deltaTime);
        RingStagingBuffer& GetStagingBuffer()
        {
            return ringStagingBuffer;
        }

    private:
        UniformArena uniformArena;
        RingStagingBuffer ringStagingBuffer;
        UIPass uiPass;
        TestPass testPass;
        DebugInfoPass debugInfoPass;

        bool isFirstFrame = true;
    };
}
