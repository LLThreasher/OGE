#include "oge/runtime/gfx/uniform_arena.hpp"

#include "oge/graphics/backend.hpp"
#include "oge/graphics/configs.hpp"
#include "oge/log.hpp"
#include "oge/math.hpp"

namespace oge::runtime::gfx
{
using namespace graphics;

FrameArena::FrameArena(BufferUsage usage) : m_usage(usage)
{
}

void FrameArena::Initialize(IGraphicsBackend& backend, uint32_t capacity)
{
    // capacity = backend.MaxUniformBufferSize() > capacity ? capacity : backend.MaxUniformBufferSize();
    m_capacityPerFrame = capacity;
    m_framesInFlight = backend.FramesInFlight();
    m_alignment = m_usage == BufferUsage::Uniform ? backend.UniformBufferAlignment() : 4;
    m_alignedCapacityPerFrame = math::align(m_capacityPerFrame, m_alignment);

    BufferDesc desc{};
    desc.memory = MemoryUsage::CPUToGPU;
    desc.usage = m_usage | BufferUsage::TransferDst;
    desc.size = m_capacityPerFrame * backend.FramesInFlight();
    m_gpuBuffer = backend.CreateBuffer(desc, &m_cpuBuffer);
}

void FrameArena::Shutdown(IGraphicsBackend& backend)
{
    backend.DestroyBuffer(m_gpuBuffer);
    m_cpuBuffer = nullptr;
}

GPUBufferHandle FrameArena::GetBuffer() { return m_gpuBuffer; }

void FrameArena::AdvanceFrame()
{
    m_frameIdx = (m_frameIdx + 1) % m_framesInFlight;
    m_head = m_frameIdx * m_alignedCapacityPerFrame;
}

StagingAllocation FrameArena::Allocate(uint32_t size)
{
    uint32_t alignedSize = math::align(size, m_alignment);

    uint32_t frameBase = m_frameIdx * m_alignedCapacityPerFrame;
    uint32_t frameEnd = frameBase + m_alignedCapacityPerFrame;

    assert(m_head + alignedSize <= frameEnd);

    StagingAllocation result{};
    result.offset = m_head;
    result.size = alignedSize;
    result.cpuPtr = static_cast<uint8_t*>(m_cpuBuffer) + m_head;

    m_head += alignedSize;

    return result;
}

void FrameArena::Flush(IGraphicsBackend& backend)
{
    GPUBufferSpan range;
    range.buffer = m_gpuBuffer;
    range.offset = 0;
    range.size = m_head;
    backend.FlushStagingBufferRanges(std::span(&range, 1));
}
}  // namespace oge::runtime
