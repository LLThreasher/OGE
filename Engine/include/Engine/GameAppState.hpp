#pragma once

#include <entt/entt.hpp>
#include "Engine/AssetBundle.hpp"
#include "Engine/TickScheduler.hpp"
#include "Engine/Input/InputSystem.hpp"

namespace OneGame::Engine
{
    class AssetManager;
    class StreamingManager;
    namespace Graphics
    {
        class Renderer;
    }

    enum class AppFrameAction : uint32_t
    {
        None = 0,
        WaitSurface = 1,
        WrapMouse = 1 << 1,
        UnwrapMouse = 1 << 2,
    };

    inline AppFrameAction operator&(AppFrameAction a, AppFrameAction b)
    {
        return static_cast<AppFrameAction>(
            static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
    }

    inline AppFrameAction operator|(AppFrameAction a, AppFrameAction b)
    {
        return static_cast<AppFrameAction>(
            static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }

    enum class SceneActionType : uint32_t
    {
        SetMouseWarpping
    };

    struct SetMouseWarppingAction
    {
        bool enabled;
    };

    struct SceneAction
    {
        SceneActionType type;
        union
        {
            SetMouseWarppingAction setMouseWarpping;
        };
    };

    struct AppContext
    {
        Graphics::IGraphicsBackend& backend;
        AssetManager& assetManager;
        StreamingManager& streamingManager;
        Graphics::Renderer& renderer;
        entt::dispatcher& events;

        AssetBundleWriter CreateAssetBundle()
        {
            return AssetBundleWriter(assetManager, streamingManager, backend);
        }
    };

    struct FrameContext
    {
        float dt;
        entt::registry& presentationWorld;
        InputSystem& input;
        std::vector<SceneAction>& outSceneActions;
    };
}
