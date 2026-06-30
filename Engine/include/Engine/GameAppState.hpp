#pragma once

#include "Engine/AssetBundle.hpp"
#include "Engine/TickScheduler.hpp"
#include "Engine/entt.hpp"
#include "Engine/Point2.hpp"
#include "Engine/Math.hpp"

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

struct AssetBase
{
    AssetManager& assetManager;

    bool LoadBlob(const std::string_view& id, std::vector<char>& data);
};

// Application states
struct AppContext : AssetBase
{
    entt::dispatcher& events;
    const Graphics::IGraphicsBackend* backend = nullptr;
};

// world states are stored in scenes

struct FramePerfStatus
{
    float inputProcessingTime;
    float logicTime;
    float assetUploadTime;
    float renderSubmitTime;

    float actualFrameTime()
    {
        return inputProcessingTime + logicTime + assetUploadTime + renderSubmitTime;
    }
};

// this comes every frame
struct FrameInputData
{
    float dt;
    InputSystem& input;
    FramePerfStatus perfStats;
};

// game states writes to this
struct FrameOutputData
{
    entt::registry& presentationWorld;
    std::vector<SceneAction>& outSceneActions;
};

struct AssetContext : AssetBase
{
    StreamingManager& streamingManager;
    Graphics::IGraphicsBackend& backend;
    AssetPool& assetPool;

    GPUTextureHandle LoadTexture(const std::string_view& id);
    Mesh AllocateMesh(int vCount, int iCount);
    Mesh LoadMesh(const std::span<const std::byte> vertices, const std::span<const std::byte> indices,
                  uint32_t indexCount, ResourceBundleHandle res = {});
    template <typename TData, typename TIndex>
    Mesh LoadMesh(const std::vector<TData>& vertices, const std::vector<TIndex>& indices)
    {
        return LoadMesh(std::as_bytes(std::span{vertices}), std::as_bytes(std::span{indices}), indices.size());
    }
    template <auto uploadType>
    void UploadMesh(const std::span<const std::byte> vertices, const std::span<const std::byte> indices, Mesh& m,
                    uint32_t indexCount, ResourceBundleHandle res = {});
    template <auto uploadType, typename TData, typename TIndex>
    void UploadMesh(const std::vector<TData>& vertices, const std::vector<TIndex>& indices, Mesh& m,
                    ResourceBundleHandle res = {})
    {
        UploadMesh<uploadType>(std::as_bytes(std::span{vertices}), std::as_bytes(std::span{indices}), m, indices.size(),
                               res);
    }
};

// presentation context
// writing presentation world requires this
struct PresentationContext : AssetContext
{
    entt::dispatcher& events;
    Graphics::Renderer& renderer;

    operator AppContext() const { return {assetManager, events, &backend}; }
};

// relationship:
// frame input data + application context = game states
// game states + presentation context = frame output data
// network layer:
// server game states -> client game states

struct SurfaceRecreateEvent
{
    UPoint2 swapchainExtent;
    math::Orientation swapchainPretransform;
};
}  // namespace OneGame::Engine
