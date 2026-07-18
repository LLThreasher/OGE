#include "oge/runtime/gfx/draw_context.hpp"

#include "oge/graphics/backend.hpp"
#include "oge/graphics/command_list.hpp"
#include "oge/runtime/asset_ctx.hpp"
#include "oge/runtime/typed_registry.hpp"

namespace oge::runtime::gfx
{
using namespace oge::graphics;

InitDrawContext::InitDrawContext(OGEContextReadOnly& ctx) : assets(*ctx.Get<AssetContext>())
{
    uniformArena.Initialize(assets.backend, 1024 * 1024 * 32);
}

DrawContext::DrawContext(float dt, InitDrawContext& ctx)
    : backend(ctx.assets.backend),
      uniformArena(ctx.uniformArena),
      transferCmd(backend.CreateCommandList(QueueType::Transfer)),
      drawCmd(backend.CreateCommandList(QueueType::Present)),
      chunkAllocator(ctx.assets.chunkAllocator),
      spriteAllocator(ctx.assets.spriteAllocator),
      dt(dt)
{
}

DrawContext::~DrawContext()
{
    auto& cmd = drawCmd;
    auto& tCmd = transferCmd;

    cmd.EndRenderPass();

    tCmd.End();
    cmd.End();

    uniformArena.Flush(backend);
    uniformArena.AdvanceFrame();
}
}  // namespace oge::runtime::renderer
