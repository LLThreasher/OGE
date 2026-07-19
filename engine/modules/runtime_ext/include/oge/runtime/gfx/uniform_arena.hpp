#pragma once

#include "oge/graphics/forward.hpp"
#include "oge/graphics/objects.hpp"
#include "oge/runtime/objects_ext.hpp"

namespace oge::runtime::gfx
{
using IGraphicsBackend = graphics::IGraphicsBackend;
class UniformArena
{
   public:
    ~UniformArena() = default;

    void Initialize(IGraphicsBackend& backend, uint32_t capacityPerFrame);
    void Shutdown(IGraphicsBackend& backend);
    GPUBufferHandle GetBuffer();

    void AdvanceFrame();
    StagingAllocation Allocate(uint32_t size);

    void Flush(IGraphicsBackend& backend);

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
}  // namespace OneGame::Engine::Graphics
