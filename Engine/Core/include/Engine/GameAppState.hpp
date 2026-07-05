#pragma once

#include "Engine/TickScheduler.hpp"
#include "Engine/entt.hpp"
#include "Engine/Point2.hpp"
#include "Engine/Math.hpp"

namespace OneGame::Engine
{
class InputSystem;
class AssetManager;

enum class AppFrameAction : uint32_t
{
    None = 0,
    WaitSurface = 1,
    WrapMouse = 1 << 1,
    UnwrapMouse = 1 << 2,
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

struct AssetBase
{
    AssetManager& assetManager;

    bool LoadBlob(const std::string_view& id, std::vector<char>& data);
};

struct SceneContext
{
    entt::dispatcher& events;
    entt::meta_any& sceneArgs;
};

// Application states
struct AppContext : AssetBase, SceneContext
{
};

// world states are stored in scenes

struct FramePerfStatus
{
    float inputProcessingTime;
    float logicTime;
    float assetUploadTime;
    float renderSubmitTime;

    float actualFrameTime() const
    {
        return inputProcessingTime + logicTime + assetUploadTime + renderSubmitTime;
    }

    FramePerfStatus operator+(const FramePerfStatus& other) const
    {
        return FramePerfStatus{
            inputProcessingTime + other.inputProcessingTime,
            logicTime + other.logicTime,
            assetUploadTime + other.assetUploadTime,
            renderSubmitTime + other.renderSubmitTime,
        };
    }

    FramePerfStatus operator/(float divisor) const
    {
        return FramePerfStatus{
            inputProcessingTime / divisor,
            logicTime / divisor,
            assetUploadTime / divisor,
            renderSubmitTime / divisor,
        };
    }
};

struct FrameData
{
    float dt;
    FramePerfStatus perfStats;
};

// this comes every frame
struct FrameInputData : FrameData
{
    InputSystem& input;
};

// relationship:
// frame input data + application context = game states
// game states + presentation context = frame output data
// network layer:
// server game states -> client game states

}  // namespace OneGame::Engine
