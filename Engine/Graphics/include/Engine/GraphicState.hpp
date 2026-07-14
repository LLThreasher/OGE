#pragma once
#include "Engine/AssetBundle.hpp"
#include "Engine/GameAppState.hpp"
#include "Engine/Graphics/SubmissionQueue.hpp"

namespace OneGame::Engine
{

class AssetManager;
class StreamingManager;

namespace Graphics
{
class Renderer;
}

// game states writes to this
struct FrameOutputData : FrameData
{
    Graphics::SubmissionQueue& graphicQueue;
    std::vector<SceneAction>& outSceneActions;
};

namespace UI
{
class IFont;
}

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

    std::shared_ptr<UI::IFont> LoadASCIIBitmapFont16x6(const std::string_view& id);
};

// presentation context
// writing presentation world requires this
struct PresentationContext : AssetContext, SceneContext
{
    Graphics::Renderer& renderer;
    ECS::RendererRegistry& rendererRegistry;

    AppContext appCtx = {assetManager, events, sceneArgs, subsystemRegistry};

    operator AppContext&() { return appCtx; }
    operator const AppContext&() const { return appCtx; }
};

struct SurfaceRecreateEvent
{
    U16Point2 swapchainExtent;
    math::Orientation swapchainPretransform;
};
}  // namespace OneGame::Engine
