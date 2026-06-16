#include "Engine/Graphics/UniformArena.hpp"
#include "Engine/Math.hpp"

namespace OneGame::Engine::Graphics
{
	void UniformArena::Initialize(IGraphicsBackend* backend, uint32_t capacity)
	{
		m_capacityPerFrame = capacity;
		m_framesInFlight = backend->FramesInFlight();
		m_alignment = backend->UniformBufferAlignment();
		m_alignedCapacityPerFrame = math::align(m_capacityPerFrame, m_alignment);

		BufferDesc desc{};
		desc.memory = MemoryUsage::CPUToGPU;
		desc.usage = BufferUsage::Uniform | BufferUsage::TransferDst;
		desc.size = m_capacityPerFrame * backend->FramesInFlight();
		m_gpuBuffer = backend->CreateBuffer(desc, &m_cpuBuffer);
	}

	void UniformArena::Shutdown(IGraphicsBackend* backend)
	{
		backend->DestroyBuffer(m_gpuBuffer);
		m_cpuBuffer = nullptr;
	}

	GPUBufferHandle UniformArena::GetBuffer()
	{
		return m_gpuBuffer;
	}

	void UniformArena::AdvanceFrame()
	{
		m_frameIdx = (m_frameIdx + 1) % m_framesInFlight;
		m_head = m_frameIdx * m_alignedCapacityPerFrame;
	}

	UniformArena::BufferRange UniformArena::Allocate(uint32_t size)
	{
		uint32_t alignedSize = math::align(size, m_alignment);

		uint32_t frameBase = m_frameIdx * m_alignedCapacityPerFrame;
		uint32_t frameEnd = frameBase + m_alignedCapacityPerFrame;

		assert(m_head + alignedSize <= frameEnd);

		BufferRange result{};
		result.offset = m_head;
		result.size = alignedSize;
		result.cpuPtr = static_cast<uint8_t*>(m_cpuBuffer) + m_head;

		m_head += alignedSize;

		return result;
	}
}
