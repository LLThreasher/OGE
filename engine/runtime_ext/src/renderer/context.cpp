#include "oge/runtime/renderer/context.hpp"

#include "oge/graphics/backend.hpp"
#include "oge/graphics/command_list.hpp"
#include "oge/runtime/asset_ctx.hpp"

namespace oge::runtime::renderer
{
using namespace graphics;

InitDrawContext::InitDrawContext(OGEContextReadOnly& ctx) : assets(*ctx.Get<AssetContext>())
{
    uniformArena.Initialize(assets.backend, 1024 * 1024 * 32);
}

DrawContext::DrawContext(InitDrawContext& ctx)
    : backend(ctx.assets.backend),
      uniformArena(ctx.uniformArena),
      transferCmd(backend.CreateCommandList(QueueType::Transfer)),
      drawCmd(backend.CreateCommandList(QueueType::Present)),
      chunkAllocator(ctx.chunkAllocator),
      spriteAllocator(ctx.spriteAllocator)
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
