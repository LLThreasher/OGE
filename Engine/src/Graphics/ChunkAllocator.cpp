#include "Engine/Graphics/ChunkAllocator.hpp"
#include <cassert>

namespace OneGame::Engine::Graphics
{
	ChunkAllocator::ChunkAllocator(BufferUsage bufUsage, MemoryUsage memUsage, int chunkSize, int maxChunks) :
		chunkSize(chunkSize), capacity(chunkSize* maxChunks)
	{
		bufferDesc.size = capacity;
		bufferDesc.usage = bufUsage;
		bufferDesc.memory = memUsage;
	}

	void ChunkAllocator::Initialize(IGraphicsBackend* backend)
	{
		gpuBuffer = backend->CreateBuffer(bufferDesc);
	}

	void ChunkAllocator::Shutdown(IGraphicsBackend* backend)
	{
		backend->DestroyBuffer(gpuBuffer);
	}

	uint32_t ChunkAllocator::AllocateChunk(GPUBufferAllocation& alloc, uint32_t numChunks)
	{
		uint32_t result;
		if (m_freeList.size() > 0)
		{
			for (size_t i = m_freeList.size() - 1; i >= 0; i)
			{

				for (size_t j = 0; j < numChunks; ++j)
				{
					result = m_freeList[m_freeList.size() - 1];
				}
			}
			m_freeList.pop_back();
		}
		else
		{
			result = nextIdx++;
		}

		assert(result * chunkSize + chunkSize * numChunks < capacity);

		alloc = { result * chunkSize, chunkSize * numChunks };

		return result;
	}

	void ChunkAllocator::FreeChunk(uint32_t idx)
	{
		m_freeList.push_back(idx);
	}
}
