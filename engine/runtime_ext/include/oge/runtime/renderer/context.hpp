#pragma once

#include "oge/graphics/forward.hpp"
#include "oge/runtime/staged_scheduler.hpp"
#include "oge/runtime/uniform_arena.hpp"
#include "oge/runtime/renderer/chunk_allocator2.hpp"
#include "oge/runtime/renderer/skyline_allocator.hpp"

namespace oge::runtime
{
class AssetContext;

namespace renderer
{
struct InitDrawContext
{
    AssetContext& assets;
    UniformArena uniformArena;

    InitDrawContext(OGEContextReadOnly& ctx);
};

struct DrawContext
{
    IGraphicsBackend& backend;
    UniformArena& uniformArena;
    DynamicChunkAllocator& chunkAllocator;
    DynamicSkylineAllocator& spriteAllocator;
    graphics::ICommandList& transferCmd;
    graphics::ICommandList& drawCmd;

    DrawContext(InitDrawContext& ctx);
    ~DrawContext();
};
}  // namespace renderer
}  // namespace oge::runtime