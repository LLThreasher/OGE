#include "oge/runtime/gfx/uniform_arena.hpp"

#include "oge/graphics/backend.hpp"
#include "oge/graphics/configs.hpp"
#include "oge/log.hpp"
#include "oge/math.hpp"
#include "oge/runtime/objects_ext.hpp"

namespace oge::runtime::gfx
{
using namespace graphics;

FrameArena::FrameArena(BufferUsage usage) : m_usage(usage)
{
}

void FrameArena::Initialize(IGraphicsBackend& backend, uint32_t capacity)
{
    // capacity = backend.MaxUniformBufferSize() > capacity ? capacity : backend.MaxUniformBufferSize();
    m_alignment = m_usage == BufferUsage::Uniform ? backend.UniformBufferAlignment() : 4;
    m_capacityPerFrame = math::align(capacity, m_alignment);
    m_framesInFlight = backend.FramesInFlight();
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
    StagingAllocation res{};
    TryAllocate(size, res);
    return res;
}

bool FrameArena::TryAllocate(uint32_t size, StagingAllocation& alloc)
{
    uint32_t alignedSize = math::align(size, m_alignment);

    uint32_t frameBase = m_frameIdx * m_alignedCapacityPerFrame;
    uint32_t frameEnd  = frameBase + m_alignedCapacityPerFrame;

    // Ensure head is inside this frame (safety check)
    if (m_head < frameBase)
        m_head = frameBase;

    // Overflow check
    if (m_head + alignedSize > frameEnd)
        return false;

    alloc.offset = m_head;
    alloc.size   = alignedSize;
    alloc.cpuPtr = static_cast<uint8_t*>(m_cpuBuffer) + m_head;

    m_head += alignedSize;

    return true;
}

void FrameArena::Flush(IGraphicsBackend& backend)
{
    GPUBufferSpan range;
    range.buffer = m_gpuBuffer;
    range.offset = m_frameIdx * m_alignedCapacityPerFrame;
    range.size = m_head - m_frameIdx * m_alignedCapacityPerFrame;
    backend.FlushStagingBufferRanges(std::span(&range, 1));
}
}  // namespace oge::runtime
