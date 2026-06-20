#include "Engine/StreamingManager.hpp"

namespace OneGame::Engine
{
	using namespace Engine::Graphics;

	void StreamingManager::Initialize(IGraphicsBackend* backend)
	{
		m_ringStagingBuffer.Initialize(backend, m_uploadByteBudget * backend->FramesInFlight());
	}

	void StreamingManager::Shutdown(IGraphicsBackend* backend)
	{
		m_ringStagingBuffer.Shutdown(backend);
	}

	Graphics::RingStagingBuffer* StreamingManager::GetStagingBuffer()
	{
		return &m_ringStagingBuffer;
	}

	ResourceBundleHandle StreamingManager::CreateResourceBundle()
	{
		return m_resouceBundles.Create();
	}

	template<UploadType uploadType>
	void StreamingManager::ScheduleBufferUpload(const BufferUploadDesc& desc)
	{
		assert(desc.stagingAlloc.size <= m_uploadByteBudget);
		if constexpr (uploadType == UploadType::Immediate)
			m_buffersToUploadImmediate.push(desc);
		else
			m_buffersToUpload.push(desc);
	}

	template void StreamingManager::ScheduleBufferUpload<UploadType::Immediate>(const BufferUploadDesc& desc);
	template void StreamingManager::ScheduleBufferUpload<UploadType::Async>(const BufferUploadDesc& desc);

	template<UploadType uploadType>
	void StreamingManager::ScheduleTextureUpload(const TextureUploadDesc& desc)
	{
		assert(desc.stagingAlloc.size <= m_uploadByteBudget);
		if constexpr (uploadType == UploadType::Immediate)
			m_texturesToUploadImmediate.push(desc);
		else
			m_texturesToUpload.push(desc);
	}

	template void StreamingManager::ScheduleTextureUpload<UploadType::Immediate>(const TextureUploadDesc& desc);
	template void StreamingManager::ScheduleTextureUpload<UploadType::Async>(const TextureUploadDesc& desc);

	void StreamingManager::UploadBuffer(uint32_t fidx, BufferUploadDesc& desc, ICommandList* transferCmd)
	{
		transferCmd->CopyBuffer(m_ringStagingBuffer.GetBuffer(), desc.gpuBuffer, desc.effectiveBufferSize, desc.stagingAlloc.offset, desc.gpuBufferOffset);
		transferCmd->BufferBarrier(desc.gpuBuffer, desc.bufferUsage | BufferUsage::TransferDst, desc.bufferUsage);
		m_stagingAllocationToFree[fidx].emplace(desc.bundle, desc.stagingAlloc);
	}

	void StreamingManager::UploadTexture(uint32_t fidx, TextureUploadDesc& desc, ICommandList* transferCmd)
	{
		transferCmd->TextureBarrier(desc.gpuBuffer, Graphics::TextureState::TransferDst);
		transferCmd->CopyBufferToTexture(m_ringStagingBuffer.GetBuffer(), desc.gpuBuffer, desc.stagingAlloc.offset);
		transferCmd->TextureBarrier(desc.gpuBuffer, Graphics::TextureState::ShaderRead);
		m_stagingAllocationToFree[fidx].emplace(desc.bundle, desc.stagingAlloc);
	}

	void StreamingManager::RunUploadStep(const Graphics::IGraphicsBackend* backend, Graphics::ICommandList* transferCmd)
	{
		auto stagingGpuBuffer = m_ringStagingBuffer.GetBuffer();
		auto fidx = backend->CurrentFrameIndex();
		while (!m_stagingAllocationToFree[fidx].empty())
		{
			auto& [event, buffer] = m_stagingAllocationToFree[fidx].front();
			m_ringStagingBuffer.Free(buffer.offset, buffer.size);
			m_stagingAllocationToFree[fidx].pop();
			if (event.IsValid())
			{
				m_resouceBundles.Get(event)->m_itemCounter -= 1;
			}
		}

		while (!m_texturesToUploadImmediate.empty())
		{
			auto desc = std::move(m_texturesToUploadImmediate.front());
			m_texturesToUploadImmediate.pop();
			UploadTexture(fidx, desc, transferCmd);
		}

		while (!m_buffersToUploadImmediate.empty())
		{
			auto desc = std::move(m_buffersToUploadImmediate.front());
			m_buffersToUploadImmediate.pop();
			UploadBuffer(fidx, desc, transferCmd);
		}

		size_t totalBytesUploaded = 0;
		while (!m_texturesToUpload.empty() && totalBytesUploaded <= m_uploadByteBudget)
		{
			auto desc = std::move(m_texturesToUpload.front());
			m_texturesToUpload.pop();
			UploadTexture(fidx, desc, transferCmd);
			totalBytesUploaded += desc.stagingAlloc.size;
		}

		while (!m_buffersToUpload.empty() && totalBytesUploaded <= m_uploadByteBudget)
		{
			auto& desc = m_buffersToUpload.front();
			UploadBuffer(fidx, desc, transferCmd);
			m_buffersToUpload.pop();
			totalBytesUploaded += desc.stagingAlloc.size;
		}
	}

	template void StreamingManager::ScheduleBufferUpload<UploadType::Immediate>(const BufferUploadDesc& desc);
	template void StreamingManager::ScheduleBufferUpload<UploadType::Async>(const BufferUploadDesc& desc);

	template void StreamingManager::ScheduleTextureUpload<UploadType::Immediate>(const TextureUploadDesc& desc);
	template void StreamingManager::ScheduleTextureUpload<UploadType::Async>(const TextureUploadDesc& desc);
}
