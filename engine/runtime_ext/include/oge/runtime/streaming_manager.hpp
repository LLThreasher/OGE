#pragma once

#include <queue>
#include <span>

#include "oge/graphics/objects.hpp"
#include "oge/pool.hpp"
#include "oge/runtime/entt.hpp"
#include "oge/runtime/ring_staging_buffer.hpp"
#include "oge/graphics/configs.hpp"


namespace oge::runtime
{

enum class UploadType : uint32_t
{
    Async,
    Immediate,
};

enum class UploadObjectType
{
    Texture,
    Buffer,
};

struct BufferTarget
{
    graphics::BufferUsage usage;
    GPUBufferHandle buffer;
    uint32_t offset;
    uint32_t size;
};

struct TextureTarget
{
    GPUTextureHandle texture;
    URect region;
    uint32_t baseTextureLayer = 0;
    uint32_t mipLevel = 0;
};

class StreamingManager
{
    using BufferUsage = graphics::BufferUsage;
    using IGraphicsBackend = graphics::IGraphicsBackend;
    using ICommandList = graphics::ICommandList;

    struct StagingBuffer
    {
        StagingAllocation alloc;
    };

    struct BufferUploadDesc
    {
        UploadObjectType type;
        union
        {
            TextureTarget gpuTexture;
            BufferTarget gpuBuffer;
        };
        StagingBuffer staging;
        ResourceBundleHandle bundle = {};
    };

   public:
    // 4 MB per frame budget by default
    StreamingManager(size_t uploadByteBudget = 4 * 1024 * 1024) : m_uploadByteBudget(uploadByteBudget) {}
    RingStagingBuffer& GetStagingBuffer();
    void Initialize(IGraphicsBackend&);
    void Shutdown(IGraphicsBackend&);

    ResourceBundleHandle CreateResourceBundle(std::function<void()> callback);

    template <UploadType uploadType, typename TTarget>
    size_t Upload(const std::span<const std::byte> data, const TTarget target, ResourceBundleHandle resBundle = {});

    template <UploadType uploadType, typename TData>
    size_t UploadBuffer(const std::vector<TData>& data, const BufferTarget target, ResourceBundleHandle resBundle = {})
    {
        return Upload<uploadType>(std::as_bytes(std::span(data)), target, resBundle);
    }

    template <UploadType uploadType>
    size_t UploadTexture(const std::vector<char>& data, const TextureTarget target, ResourceBundleHandle resBundle = {})
    {
        return Upload<uploadType>(std::as_bytes(std::span(data)), target, resBundle);
    }

    void RunUploadStep(IGraphicsBackend&, ICommandList&);

   private:
    template <UploadType uploadType = UploadType::Immediate>
    void ScheduleBufferUpload(const BufferUploadDesc& desc);
    template <UploadType uploadType = UploadType::Immediate>
    bool AllocateStagingBuffer(const std::span<const std::byte> data, StagingBuffer&);

    void UploadBuffer(uint32_t fidx, BufferUploadDesc& desc, ICommandList& transferCmd);
    void UploadTexture(uint32_t fidx, BufferUploadDesc& desc, ICommandList& transferCmd);

    RingStagingBuffer m_ringStagingBuffer;
    size_t m_uploadByteBudget;
    std::queue<std::tuple<BufferUploadDesc, std::vector<std::byte>>> m_buffersQueuedInCPU;
    std::queue<BufferUploadDesc> m_buffersToUpload;
    std::queue<BufferUploadDesc> m_buffersToUploadImmediate;
    Pool<StreamingObjects::ResourceBundle, ResourceBundle> m_resourceBundles;
    std::unordered_map<ResourceBundleHandle, std::function<void()>, HandleHash<ResourceBundleHandle>>
        m_resourceBundleCallbacks;
    std::array<std::queue<std::tuple<ResourceBundleHandle, StagingAllocation>>, MAX_FRAMES_IN_FLIGHT>
        m_stagingAllocationToFree;
};
}  // namespace oge::runtime
