#pragma once

#include "oge/graphics/forward.hpp"
#include "oge/runtime/staged_scheduler.hpp"
#include "oge/runtime/gfx/skyline_allocator.hpp"
#include "oge/runtime/gfx/uniform_arena.hpp"
#include "oge/runtime/asset_ctx.hpp"

namespace oge::runtime
{
class OGEContextReadOnly;
namespace gfx
{
namespace dca
{
class DynamicChunkAllocator;
}
class DynamicSkylineAllocator;
} // namespace gfx
}

namespace oge::runtime
{

using namespace oge::graphics;
using namespace oge::runtime;
using namespace oge::runtime::gfx;
using DynamicChunkAllocator = oge::runtime::gfx::dca::DynamicChunkAllocator;

namespace gfx
{
struct InitDrawContext
{
    AssetContext assets;
    UniformArena uniformArena;

    InitDrawContext(OGEContextReadOnly& ctx);
};

struct DrawContext
{
    float dt;
    IGraphicsBackend& backend;
    UniformArena& uniformArena;
    DynamicChunkAllocator& chunkAllocator;
    DynamicSkylineAllocator& spriteAllocator;
    ICommandList& transferCmd;
    ICommandList& drawCmd;

    DrawContext(float dt, InitDrawContext& ctx);
    ~DrawContext();
};
}  // namespace gfx
}  // namespace game::view