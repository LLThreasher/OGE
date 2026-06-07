#pragma once

#include "IGraphicsBackend.hpp"

namespace OneGame::Engine::Graphics
{
	class UniformArena
	{
	public:
		struct BufferRange
		{
			GPUBufferHandle buffer;
			uint32_t offset;
			uint32_t size;
			void* cpuPtr;
		};
		~UniformArena() = default;

		void Initialize(IGraphicsBackend* backend, uint32_t capacityPerFrame);
		GPUBufferHandle GetGPUBuffer();

		void AdvanceFrame();
		BufferRange Allocate(uint32_t size);

	private:
		uint32_t m_capacityPerFrame;
		uint32_t m_alignedCapacityPerFrame;

		GPUBufferHandle m_gpuBuffer;
		void* m_cpuBuffer;

		uint32_t m_head = 0;
		uint32_t m_frameIdx = 0;

		uint32_t m_framesInFlight;
		uint32_t m_alignment;
	};
}
