#pragma once
#include <string>
#include <memory>
#include <entt/entt.hpp>

#include "Engine/Graphics/Renderer.hpp"
#include "Engine/AssetManager.hpp"
#include "Engine/TickScheduler.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Terrain/TerrainMeshBuilder.hpp"
#include "Engine/Scenes/IScene.hpp"
#include "Engine/GameAppState.hpp"

namespace OneGame::Engine
{
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

    class GameApp
    {
    public:
        GameApp();
        ~GameApp();

        void Initialize(Graphics::WindowHandle* handle);
        AppFrameAction Update(float dt, InputSystem& input);
        void Shutdown();

        void OnWindowRecreate(Graphics::WindowHandle*);
        void OnResize(int width, int height);
    private:
        void TransferToScene(uint32_t nextScene);

        std::unique_ptr<Graphics::IGraphicsBackend> backend;
        AssetManager assetManager;
        StreamingManager streamingManager;
        Graphics::Renderer renderer;
        entt::dispatcher dispatcher;

        entt::registry world;

        std::vector<std::unique_ptr<IScene>> allScenes;
        IScene* nextScene = nullptr;
        IScene* currentScene = nullptr;
    };
}
