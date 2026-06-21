#pragma once

#include <queue>
#include <entt/entt.hpp>
#include <span>
#include "Engine/Graphics/IGraphicsBackend.hpp"
#include "Engine/Graphics/RingStagingBuffer.hpp"
#include "Engine/ResourcePool.hpp"

namespace OneGame::Engine
{

	enum class StreamingObjects
	{
		ResourceBundle,
	};

	using ResourceBundleHandle = ResourceHandle<StreamingObjects::ResourceBundle>;


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
		size_t m_itemCounter = 0;
		std::function<void()> m_callback = nullptr;
	};

	class StreamingManager
	{
		using RingStagingBuffer = Graphics::RingStagingBuffer;
		using StagingAllocation = Graphics::StagingAllocation;
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
			union {
				GPUTextureHandle gpuTexture;
				GPUBufferHandle gpuBuffer;
			};
			size_t gpuBufferOffset;
			StagingBuffer staging;
			ResourceBundleHandle bundle = {};
		};

	public:
		// 4 MB per frame budget by default
		StreamingManager(size_t uploadByteBudget = 4 * 1024 * 1024) : m_uploadByteBudget(uploadByteBudget)
		{
		}
		RingStagingBuffer& GetStagingBuffer();
		void Initialize(Graphics::IGraphicsBackend&);
		void Shutdown(Graphics::IGraphicsBackend&);

		ResourceBundleHandle CreateResourceBundle(std::function<void()> callback);

		template<UploadType uploadType, BufferUsage usage, typename THandle>
		size_t UploadBuffer(const std::span<const std::byte> data, const THandle handle, const size_t gpuOffset = 0, ResourceBundleHandle resBundle = {});

		template<UploadType uploadType, BufferUsage usage, typename TData>
		size_t UploadBuffer(const std::vector<TData>& data, const GPUBufferHandle handle, const size_t gpuOffset = 0, ResourceBundleHandle resBundle = {})
		{
			return UploadBuffer<uploadType, usage>(std::as_bytes(std::span(data)), handle, gpuOffset, resBundle);
		}

		template<UploadType uploadType>
		size_t UploadTexture(const std::vector<char>& data, const GPUTextureHandle handle, const size_t gpuOffset = 0, ResourceBundleHandle resBundle = {})
		{
			return UploadBuffer<uploadType, BufferUsage::None>(std::as_bytes(std::span(data)), handle, gpuOffset, resBundle);
		}

		void RunUploadStep(const Graphics::IGraphicsBackend&, Graphics::ICommandList&);

	private:
		template<UploadType uploadType = UploadType::Immediate>
		void ScheduleBufferUpload(const BufferUploadDesc& desc);
		template<UploadType uploadType = UploadType::Immediate>
		StagingBuffer AllocateStagingBuffer(const std::span<const std::byte> data);

		void UploadBuffer(uint32_t fidx, BufferUploadDesc& desc, Graphics::ICommandList& transferCmd);
		void UploadTexture(uint32_t fidx, BufferUploadDesc& desc, Graphics::ICommandList& transferCmd);

		RingStagingBuffer m_ringStagingBuffer;
		size_t m_uploadByteBudget;
		std::queue<std::tuple<BufferUploadDesc, std::vector<std::byte>>> m_buffersQueuedInCPU;
		std::queue<BufferUploadDesc> m_buffersToUpload;
		std::queue<BufferUploadDesc> m_buffersToUploadImmediate;
		ResourcePool<StreamingObjects::ResourceBundle, ResourceBundle> m_resouceBundles;
		std::array<std::queue<std::tuple<ResourceBundleHandle, StagingAllocation>>, MAX_FRAMES_IN_FLIGHT> m_stagingAllocationToFree;
	};
}
