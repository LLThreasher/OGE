#pragma once

#include <queue>
#include <entt/entt.hpp>
#include <span>
#include "Engine/Graphics/IGraphicsBackend.hpp"
#include "Engine/Graphics/RingStagingBuffer.hpp"
#include "Engine/ResourcePool.hpp"

namespace OneGame::Engine
{
	using StagingAllocation = Graphics::StagingAllocation;
	using BufferUsage = Graphics::BufferUsage;

	enum class StreamingObjects
	{
		ResourceBundle,
	};

	using ResourceBundleHandle = ResourceHandle<StreamingObjects::ResourceBundle>;

	struct BufferUploadDesc
	{
		BufferUsage bufferUsage;
		size_t effectiveBufferSize;
		GPUBufferHandle gpuBuffer;
		size_t gpuBufferOffset;
		Graphics::StagingAllocation stagingAlloc;
		ResourceBundleHandle bundle = {};
	};

	struct MeshUploadDesc
	{
		BufferUploadDesc vertexUploadDesc;
		BufferUploadDesc indexUploadDesc;
	};

	struct TextureUploadDesc
	{
		GPUTextureHandle gpuBuffer;
		size_t gpuBufferOffset;
		StagingAllocation stagingAlloc;
		ResourceBundleHandle bundle = {};
	};

	enum class UploadType : uint32_t
	{
		Async,
		Immediate,
	};

	struct ResourceBundle
	{
		size_t m_itemCounter = 0;
		std::function<void()> m_callback = nullptr;
	};

	class StreamingManager
	{
		friend class ResourceBundle;

	public:
		// 4 MB per frame budget by default
		StreamingManager(size_t uploadByteBudget = 4 * 1024 * 1024) : m_uploadByteBudget(uploadByteBudget)
		{
		}
		Graphics::RingStagingBuffer* GetStagingBuffer();
		void Initialize(Graphics::IGraphicsBackend*);
		void Shutdown(Graphics::IGraphicsBackend*);

		ResourceBundleHandle CreateResourceBundle();

		template<UploadType uploadType = UploadType::Immediate>
		void ScheduleBufferUpload(const BufferUploadDesc& desc);

		template<UploadType uploadType = UploadType::Immediate>
		void ScheduleTextureUpload(const TextureUploadDesc& desc);

		template<UploadType uploadType, BufferUsage usage, typename TData>
		size_t UploadBuffer(const std::vector<TData> data, const GPUBufferHandle handle, const size_t gpuOffset = 0, ResourceBundleHandle resBundle = {})
		{
			return UploadBuffer<uploadType, usage, TData>(data, handle, gpuOffset, resBundle);
		}

		template<UploadType uploadType, BufferUsage usage, typename TData, size_t extend>
		size_t UploadBuffer(const std::span<TData, extend> data, const GPUBufferHandle handle, const size_t gpuOffset = 0, ResourceBundleHandle resBundle = {})
		{
			size_t dataSizeInBytes = data.size() * sizeof(TData);
			StagingAllocation alloc = m_ringStagingBuffer.Allocate(dataSizeInBytes);
			memcpy(alloc.cpuPtr, data.data(), dataSizeInBytes);
			BufferUploadDesc desc{};
			desc.bundle = resBundle;
			desc.effectiveBufferSize = dataSizeInBytes;
			desc.gpuBuffer = handle;
			desc.gpuBufferOffset = gpuOffset;
			desc.stagingAlloc = alloc;
			desc.bufferUsage = usage;
			ScheduleBufferUpload<uploadType>(desc);
			return dataSizeInBytes;
		}

		template<UploadType uploadType, typename TData, size_t extend>
		size_t UploadTexture(const std::span<TData, extend> data, const GPUTextureHandle handle, const size_t gpuOffset = 0, ResourceBundleHandle resBundle = {})
		{
			size_t dataSizeInBytes = data.size() * sizeof(TData);
			StagingAllocation alloc = m_ringStagingBuffer.Allocate(dataSizeInBytes);
			memcpy(alloc.cpuPtr, data.data(), dataSizeInBytes);
			TextureUploadDesc desc{};
			desc.bundle = resBundle;
			desc.gpuBuffer = handle;
			desc.gpuBufferOffset = gpuOffset;
			desc.stagingAlloc = alloc;
			ScheduleTextureUpload<uploadType>(desc);
			return dataSizeInBytes;
		}

		void RunUploadStep(const Graphics::IGraphicsBackend*, Graphics::ICommandList*);
		void SetCallback(ResourceBundleHandle handle, std::function<void()> callback);
		void ExecuteCallbacks();

	private:
		void UploadBuffer(uint32_t fidx, BufferUploadDesc& desc, Graphics::ICommandList* transferCmd);
		void UploadTexture(uint32_t fidx, TextureUploadDesc& desc, Graphics::ICommandList* transferCmd);

		Graphics::RingStagingBuffer m_ringStagingBuffer;
		size_t m_uploadByteBudget;
		std::queue<MeshUploadDesc> m_meshesToUpload;
		std::queue<BufferUploadDesc> m_buffersToUpload;
		std::queue<TextureUploadDesc> m_texturesToUpload;
		std::queue<BufferUploadDesc> m_buffersToUploadImmediate;
		std::queue<TextureUploadDesc> m_texturesToUploadImmediate;
		ResourcePool<StreamingObjects::ResourceBundle, ResourceBundle> m_resouceBundles;
		std::array<std::queue<std::tuple<ResourceBundleHandle, StagingAllocation>>, MAX_FRAMES_IN_FLIGHT> m_stagingAllocationToFree;
	};
}
