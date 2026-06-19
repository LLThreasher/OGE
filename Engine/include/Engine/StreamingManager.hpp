#pragma once

#include <queue>
#include <entt/entt.hpp>
#include "Engine/Graphics/IGraphicsBackend.hpp"
#include "Engine/Graphics/RingStagingBuffer.hpp"

namespace OneGame::Engine
{
	using StagingAllocation = Graphics::StagingAllocation;

	struct ResourceBundleEvent
	{
		size_t itemCount = 0;
	};

	struct BufferUploadDesc
	{
		size_t effectiveBufferSize;
		Graphics::BufferUsage bufferUsage;
		GPUBufferHandle gpuBuffer;
		size_t gpuBufferOffset;
		Graphics::StagingAllocation stagingAlloc;
		std::shared_ptr<ResourceBundleEvent> bundleEvent = nullptr;
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
		Graphics::StagingAllocation stagingAlloc;
		std::shared_ptr<ResourceBundleEvent> bundleEvent = nullptr;
	};

	enum class UploadType : uint32_t
	{
		Async,
		Immediate,
	};

	class StreamingManager
	{
	public:
		// 4 MB per frame budget by default
		StreamingManager(size_t uploadByteBudget = 4 * 1024 * 1024) : m_uploadByteBudget(uploadByteBudget)
		{
		}
		Graphics::RingStagingBuffer* GetStagingBuffer();
		void Initialize(Graphics::IGraphicsBackend*);
		void Shutdown(Graphics::IGraphicsBackend*);
		std::shared_ptr<ResourceBundleEvent> CreateResourceBundle();
		void ScheduleBufferUpload(const BufferUploadDesc& desc, UploadType uploadType);
		void ScheduleTextureUpload(const TextureUploadDesc& desc, UploadType uploadType);
		void RunUploadStep(const Graphics::IGraphicsBackend*, Graphics::ICommandList*, entt::dispatcher* dispatcher);
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
		std::array<std::queue<std::tuple<std::shared_ptr<ResourceBundleEvent>, StagingAllocation>>, MAX_FRAMES_IN_FLIGHT> m_stagingAllocationToFree;
	};
}
