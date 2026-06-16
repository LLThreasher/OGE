#pragma once
#include <string>
#include <memory>
#include <entt/entt.hpp>

#include "Engine/AssetManager.hpp"
#include "Engine/Graphics/Renderer.hpp"
#include "Engine/TickScheduler.hpp"
#include "Engine/Input/InputSystem.hpp"

namespace OneGame::Engine
{
    using namespace Graphics;

    namespace Graphics
    {
        class WindowHandle;
        class IGraphicsBackend;
    }

    class IGameApp
    {
    public:
        virtual void Initialize(const Graphics::WindowHandle&) = 0;
        virtual void Shutdown() = 0;

        virtual void Update(float dt) = 0;

        virtual void OnResize(int width, int height) = 0;
    };

    struct AppContext
    {
        entt::registry& world;
        Renderer& renderer;
        AssetManager& assets;
        TickScheduler& tickScheduler;
        entt::dispatcher& events;
        InputSystem& input;
    };

    class GameApp
    {
    public:
        ~GameApp();

        void Initialize(Graphics::WindowHandle*);
        void Shutdown();

        void Update(float dt);

        void OnResize(int width, int height);
    private:
        std::unique_ptr<Graphics::IGraphicsBackend> backend;
        entt::registry world;
        AssetManager assetManager;
        Renderer renderer;

        GPUInfo gpuInfo;
        entt::entity debugInfoEntity;

        float currentFPS;
        float accumTime = 0.f;
        uint64_t frameCount = 0;
    };
}
