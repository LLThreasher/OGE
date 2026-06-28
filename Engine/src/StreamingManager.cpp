#include "Engine/StreamingManager.hpp"

#define LOGGER_NAME "Engine"
#include "Engine/Logger.hpp"

namespace OneGame::Engine
{
using namespace Engine::Graphics;

void StreamingManager::Initialize(IGraphicsBackend& backend)
{
    m_ringStagingBuffer.Initialize(backend, m_uploadByteBudget * backend.FramesInFlight());
}

void StreamingManager::Shutdown(IGraphicsBackend& backend) { m_ringStagingBuffer.Shutdown(backend); }

Graphics::RingStagingBuffer& StreamingManager::GetStagingBuffer() { return m_ringStagingBuffer; }

ResourceBundleHandle StreamingManager::CreateResourceBundle(std::function<void()> callback)
{
    auto ret = m_resourceBundles.Create();
    m_resourceBundleCallbacks[ret] = callback;
    return ret;
}

template <UploadType uploadType>
void StreamingManager::ScheduleBufferUpload(const BufferUploadDesc& desc)
{
    assert(desc.staging.alloc.size <= m_uploadByteBudget);
    if constexpr (uploadType == UploadType::Immediate)
        m_buffersToUploadImmediate.push(desc);
    else
        m_buffersToUpload.push(desc);
}

template <UploadType uploadType, BufferUsage usage, typename THandle>
size_t StreamingManager::UploadBuffer(const std::span<const std::byte> data, const THandle handle,
                                      const size_t gpuOffset, ResourceBundleHandle resBundle)
{
    size_t dataSizeInBytes = data.size();
    BufferUploadDesc desc{};
    desc.bundle = resBundle;
    desc.effectiveBufferSize = dataSizeInBytes;
    if constexpr (std::is_same_v<THandle, GPUBufferHandle>)
    {
        desc.gpuBuffer = handle;
        desc.type = UploadObjectType::Buffer;
    }
    else
    {
        desc.gpuTexture = handle;
        desc.type = UploadObjectType::Texture;
    }
    desc.gpuBufferOffset = gpuOffset;
    desc.bufferUsage = usage;

    if (!AllocateStagingBuffer<uploadType>(data, desc.staging))
    {
        auto& [_desc, _] = m_buffersQueuedInCPU.back();
        _desc = desc;
    }
    else
    {
        if (!m_buffersQueuedInCPU.empty())
        {
            auto& [_desc, _] = m_buffersQueuedInCPU.back();
            assert(_desc.gpuBuffer.IsValid());
        }
        if constexpr (uploadType == UploadType::Immediate)
        {
            m_buffersToUploadImmediate.push(desc);
        }
        else
        {
            m_buffersToUpload.push(desc);
        }
    }
    if (resBundle.IsValid())
    {
        m_resourceBundles.Get(resBundle)->itemCounter += 1;
    }
    return dataSizeInBytes;
}

template <UploadType uploadType>
bool StreamingManager::AllocateStagingBuffer(const std::span<const std::byte> data,
                                             StreamingManager::StagingBuffer& res)
{
    size_t dataSizeInBytes = data.size();
    assert(dataSizeInBytes <= m_uploadByteBudget && "manumal chunking required");
    if (!m_ringStagingBuffer.TryAllocate(dataSizeInBytes, res.alloc) || !m_buffersQueuedInCPU.empty())
    {
        if constexpr (uploadType == UploadType::Immediate)
        {
            assert(false &&
                   "immediate upload overflows staging buffer, switch to "
                   "async to avoid this");
        }
        else
        {
            // allocate extra staging memory on cpu
            m_buffersQueuedInCPU.push({{}, {}});
            auto& [_, buf] = m_buffersQueuedInCPU.back();
            buf.resize(dataSizeInBytes);
            memcpy(buf.data(), data.data(), dataSizeInBytes);
        }
        return false;
    }
    else
    {
        memcpy(res.alloc.cpuPtr, data.data(), dataSizeInBytes);
        return true;
    }
}

void StreamingManager::UploadBuffer(uint32_t fidx, BufferUploadDesc& desc, ICommandList& transferCmd)
{
    transferCmd.CopyBuffer(m_ringStagingBuffer.GetBuffer(), desc.gpuBuffer, desc.effectiveBufferSize,
                           desc.staging.alloc.offset, desc.gpuBufferOffset);
    transferCmd.BufferBarrier(desc.gpuBuffer, desc.bufferUsage | BufferUsage::TransferDst, desc.bufferUsage);
    m_stagingAllocationToFree[fidx].emplace(desc.bundle, desc.staging.alloc);
}

void StreamingManager::UploadTexture(uint32_t fidx, BufferUploadDesc& desc, ICommandList& transferCmd)
{
    transferCmd.TextureBarrier(desc.gpuTexture, Graphics::TextureState::TransferDst);
    transferCmd.CopyBufferToTexture(m_ringStagingBuffer.GetBuffer(), desc.gpuTexture, desc.staging.alloc.offset);
    transferCmd.TextureBarrier(desc.gpuTexture, Graphics::TextureState::ShaderRead);
    m_stagingAllocationToFree[fidx].emplace(desc.bundle, desc.staging.alloc);
}

void StreamingManager::RunUploadStep(Graphics::IGraphicsBackend& backend, Graphics::ICommandList& transferCmd)
{
    auto stagingGpuBuffer = m_ringStagingBuffer.GetBuffer();
    auto fidx = backend.CurrentFrameIndex();
    while (!m_stagingAllocationToFree[fidx].empty())
    {
        // LOG_DEBUG("checking to free {} {}", fidx,
        // m_stagingAllocationToFree[fidx].size());
        auto& [event, buffer] = m_stagingAllocationToFree[fidx].front();
        m_ringStagingBuffer.Free(buffer.offset, buffer.size);
        m_stagingAllocationToFree[fidx].pop();
        if (event.IsValid())
        {
            auto eData = m_resourceBundles.Get(event);
            assert(eData != nullptr);
            eData->itemCounter -= 1;
            if (eData->itemCounter == 0)
            {
                auto it = m_resourceBundleCallbacks.find(event);
                if (it != m_resourceBundleCallbacks.end())
                    it->second();
                m_resourceBundles.Destroy(event);
            }
            // LOG_DEBUG("progress {}", eData->m_itemCounter);
        }
    }

    while (!m_buffersQueuedInCPU.empty())
    {
        auto& [item, buffer] = m_buffersQueuedInCPU.front();
        if (!m_ringStagingBuffer.TryAllocate(buffer.size(), item.staging.alloc)) break;
        assert(item.gpuBuffer.IsValid());
        memcpy(item.staging.alloc.cpuPtr, buffer.data(), buffer.size());
        m_buffersQueuedInCPU.pop();
        m_buffersToUpload.push(item);
    }

    while (!m_buffersToUploadImmediate.empty())
    {
        auto desc = std::move(m_buffersToUploadImmediate.front());
        m_buffersToUploadImmediate.pop();
        if (desc.type == UploadObjectType::Texture)
            UploadTexture(fidx, desc, transferCmd);
        else
            UploadBuffer(fidx, desc, transferCmd);
    }

    size_t totalBytesUploaded = 0;
    while (!m_buffersToUpload.empty() && totalBytesUploaded <= m_uploadByteBudget)
    {
        auto& desc = m_buffersToUpload.front();
        UploadBuffer(fidx, desc, transferCmd);
        m_buffersToUpload.pop();
        totalBytesUploaded += desc.staging.alloc.size;
    }

    m_ringStagingBuffer.Flush(backend);
}

template bool StreamingManager::AllocateStagingBuffer<UploadType::Immediate>(const std::span<const std::byte> data,
                                                                             StreamingManager::StagingBuffer&);
template bool StreamingManager::AllocateStagingBuffer<UploadType::Async>(const std::span<const std::byte> data,
                                                                         StreamingManager::StagingBuffer&);

template void StreamingManager::ScheduleBufferUpload<UploadType::Immediate>(const BufferUploadDesc& desc);
template void StreamingManager::ScheduleBufferUpload<UploadType::Async>(const BufferUploadDesc& desc);

template size_t StreamingManager::UploadBuffer<UploadType::Immediate, BufferUsage::Vertex>(
    const std::span<const std::byte> data, const GPUBufferHandle handle, const size_t gpuOffset = 0,
    ResourceBundleHandle resBundle = {});
template size_t StreamingManager::UploadBuffer<UploadType::Async, BufferUsage::Vertex>(
    const std::span<const std::byte> data, const GPUBufferHandle handle, const size_t gpuOffset = 0,
    ResourceBundleHandle resBundle = {});
template size_t StreamingManager::UploadBuffer<UploadType::Immediate, BufferUsage::Index>(
    const std::span<const std::byte> data, const GPUBufferHandle handle, const size_t gpuOffset = 0,
    ResourceBundleHandle resBundle = {});
template size_t StreamingManager::UploadBuffer<UploadType::Async, BufferUsage::Index>(
    const std::span<const std::byte> data, const GPUBufferHandle handle, const size_t gpuOffset = 0,
    ResourceBundleHandle resBundle = {});
template size_t StreamingManager::UploadBuffer<UploadType::Immediate, BufferUsage::Storage>(
    const std::span<const std::byte> data, const GPUBufferHandle handle, const size_t gpuOffset = 0,
    ResourceBundleHandle resBundle = {});
template size_t StreamingManager::UploadBuffer<UploadType::Async, BufferUsage::Storage>(
    const std::span<const std::byte> data, const GPUBufferHandle handle, const size_t gpuOffset = 0,
    ResourceBundleHandle resBundle = {});

template size_t StreamingManager::UploadBuffer<UploadType::Immediate, BufferUsage::None>(
    const std::span<const std::byte> data, const GPUTextureHandle handle, const size_t gpuOffset = 0,
    ResourceBundleHandle resBundle = {});
template size_t StreamingManager::UploadBuffer<UploadType::Async, BufferUsage::None>(
    const std::span<const std::byte> data, const GPUTextureHandle handle, const size_t gpuOffset = 0,
    ResourceBundleHandle resBundle = {});
}  // namespace OneGame::Engine
