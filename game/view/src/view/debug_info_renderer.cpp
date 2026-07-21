#include <format>

#include "game/components.hpp"
#include "game/view/renderer.hpp"
#include "game/view/submission_queue.hpp"
#include "oge/graphics/backend.hpp"
#include "oge/point2.hpp"
#include "oge/runtime/asset_ctx.hpp"
#include "oge/runtime/entt.hpp"

namespace game::view
{
void DebugInfoRenderer::onAttach(RendererState& ctx)
{
    debugFont = ctx.assets.LoadASCIIBitmapFont16x6("om_tall_plain_idx.png");
}

void DebugInfoRenderer::onDetach(RendererState& ctx) {}
void DebugInfoRenderer::onUpdate(FRendererState& ctx)
{
    auto gpuInfo = ctx.assets.backend.GetGPUInfo();
    m_duration += ctx.dt;
    if (m_duration > 1.f)
    {
        m_duration = 0.f;
        auto memUsage = ctx.assets.backend.GetGPUMemoryUsage();
        m_currentGPUMem = memUsage.heapUsage[0];
        m_budgetGPUMem = memUsage.heapBudget[0];
    }

    auto spriteQueue = ctx.submissionQueue.View<oge::runtime::gfx::CmdDrawSprite>().GetSingle(GameViewType::Overlay);

    std::stringstream ss;
    for (auto [e, txt] : ctx.world.view<const DebugText>().each())
    {
        ss << txt.text << std::endl;
    }
    ss << gpuInfo.name << std::endl;
    ss << std::format("GPU MEM: {} / {} MB", m_currentGPUMem / 1024 / 1024, m_budgetGPUMem / 1024 / 1024) << std::endl;
    std::string s = ss.str();

    debugFont->CreateTextSprites(spriteQueue, {s}, {{{10, 10}, ctx.assets.backend.SwapchainExtent() - oge::U16Point2{10u, 10u}}});
}
}  // namespace game::view
