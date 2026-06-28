#pragma once

#include "Engine/AssetBundle.hpp"
#include "Engine/TickScheduler.hpp"
#include "Engine/entt.hpp"

namespace OneGame::Engine
{
class AssetManager;
class StreamingManager;
class InputSystem;

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
    WaitFPS30 = 1 << 3,
    WaitFPS60 = 1 << 4,
};

inline AppFrameAction operator&(AppFrameAction a, AppFrameAction b)
{
    return static_cast<AppFrameAction>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline AppFrameAction operator|(AppFrameAction a, AppFrameAction b)
{
    return static_cast<AppFrameAction>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
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

// Application states
struct AppContext
{
    AssetManager& assetManager;
    entt::dispatcher& events;
};

// world states are stored in scenes

// this comes every frame
struct FrameInputData
{
    float dt;
    InputSystem& input;
};

// game states writes to this
struct FrameOutputData
{
    entt::registry& presentationWorld;
    std::vector<SceneAction>& outSceneActions;
};

// presentation context
// writing presentation world requires this
struct PresentationContext
{
    AppContext& appCtx;
    Graphics::IGraphicsBackend& backend;
    Graphics::Renderer& renderer;
    StreamingManager& streamingManager;
    AssetPool& assetPool;
};

// relationship:
// frame input data + application context = game states
// game states + presentation context = frame output data
// network layer:
// server game states -> client game states

struct SurfaceRecreateEvent
{
    float swapchainWidth;
    float swapchainHeight;
};
}  // namespace OneGame::Engine
