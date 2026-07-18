#include <format>

#include "game/components.hpp"
#include "game/view/renderer.hpp"
#include "oge/graphics/backend.hpp"
#include "oge/runtime/asset_ctx.hpp"
#include "oge/runtime/entt.hpp"

namespace game::view
{
void DebugInfoRenderer::onAttach(RendererState& ctx) {}
void DebugInfoRenderer::onDetach(RendererState& ctx) {}
void DebugInfoRenderer::onUpdate(FRendererState& ctx)
{
    auto gpuInfo = ctx.assets.backend.GetGPUInfo();
    auto memUsage = ctx.assets.backend.GetGPUMemoryUsage();

    ctx.submissionQueue.Add<CmdDrawDebugText>(GameViewType::Overlay, gpuInfo.name);
    for (auto [e, txt] : ctx.world.view<const DebugText, Ready>().each())
    {
        ctx.submissionQueue.Add<CmdDrawDebugText>(GameViewType::Overlay, txt.text);
    }
}
}  // namespace game::view
