#pragma once

#include <queue>
#include <span>

#include "Engine/Graphics/IGraphicsBackend.hpp"
#include "Engine/Graphics/RingStagingBuffer.hpp"
#include "Engine/ResourcePool.hpp"
#include "Engine/entt.hpp"
#include "Engine/Async.hpp"

namespace OneGame::Engine
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

struct ResourceBundle
{
  size_t itemCounter = 0;
};

class StreamingManager
{
  using RingStagingBuffer = Graphics::RingStagingBuffer;
  using BufferUsage = Graphics::BufferUsage;

  struct StagingBuffer
  {
    StagingAllocation alloc;
  };

  struct BufferUploadDesc
  {
    UploadObjectType type;
    BufferUsage bufferUsage;
    size_t effectiveBufferSize;
    union
    {
      GPUTextureHandle gpuTexture;
      GPUBufferHandle gpuBuffer;
    };
    size_t gpuBufferOffset;
    StagingBuffer staging;
    ResourceBundleHandle bundle = {};
  };

 public:
  // 4 MB per frame budget by default
  StreamingManager(size_t uploadByteBudget = 4 * 1024 * 1024) : m_uploadByteBudget(uploadByteBudget) {}
  RingStagingBuffer& GetStagingBuffer();
  void Initialize(Graphics::IGraphicsBackend&);
  void Shutdown(Graphics::IGraphicsBackend&);

  ResourceBundleHandle CreateResourceBundle(std::function<void()> callback);

  template <UploadType uploadType, BufferUsage usage, typename THandle>
  size_t UploadBuffer(const std::span<const std::byte> data, const THandle handle, const size_t gpuOffset = 0,
                      ResourceBundleHandle resBundle = {});

  template <UploadType uploadType, BufferUsage usage, typename TData>
  size_t UploadBuffer(const std::vector<TData>& data, const GPUBufferHandle handle, const size_t gpuOffset = 0,
                      ResourceBundleHandle resBundle = {})
  {
      return UploadBuffer<uploadType, usage>(std::as_bytes(std::span(data)), handle, gpuOffset, resBundle);
  }

  template <UploadType uploadType>
  size_t UploadTexture(const std::vector<char>& data, const GPUTextureHandle handle, const size_t layer = 0,
                       ResourceBundleHandle resBundle = {})
  {
    return UploadBuffer<uploadType, BufferUsage::None>(std::as_bytes(std::span(data)), handle, layer, resBundle);
  }

  void RunUploadStep(Graphics::IGraphicsBackend&, Graphics::ICommandList&);

 private:
  template <UploadType uploadType = UploadType::Immediate>
  void ScheduleBufferUpload(const BufferUploadDesc& desc);
  template <UploadType uploadType = UploadType::Immediate>
  bool AllocateStagingBuffer(const std::span<const std::byte> data, StagingBuffer&);

  void UploadBuffer(uint32_t fidx, BufferUploadDesc& desc, Graphics::ICommandList& transferCmd);
  void UploadTexture(uint32_t fidx, BufferUploadDesc& desc, Graphics::ICommandList& transferCmd);

  RingStagingBuffer m_ringStagingBuffer;
  size_t m_uploadByteBudget;
  std::queue<std::tuple<BufferUploadDesc, std::vector<std::byte>>> m_buffersQueuedInCPU;
  std::queue<BufferUploadDesc> m_buffersToUpload;
  std::queue<BufferUploadDesc> m_buffersToUploadImmediate;
  ResourcePool<StreamingObjects::ResourceBundle, ResourceBundle> m_resourceBundles;
  std::unordered_map<ResourceBundleHandle, std::function<void()>, HandleHash<ResourceBundleHandle>> m_resourceBundleCallbacks;
  std::array<std::queue<std::tuple<ResourceBundleHandle, StagingAllocation>>, MAX_FRAMES_IN_FLIGHT>
      m_stagingAllocationToFree;
};
}  // namespace OneGame::Engine
