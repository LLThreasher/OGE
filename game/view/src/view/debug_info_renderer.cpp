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
    m_duration += ctx.dt;
    if (m_duration > 1.f)
    {
        m_duration = 0.f;
        auto memUsage = ctx.assets.backend.GetGPUMemoryUsage();
        m_currentGPUMem = memUsage.heapUsage[0];
        m_budgetGPUMem = memUsage.heapBudget[0];
    }

    ctx.submissionQueue.Add<CmdDrawDebugText>(GameViewType::Overlay, gpuInfo.name);
    ctx.submissionQueue.Add<CmdDrawDebugText>(GameViewType::Overlay, std::format("GPU MEM: {} / {} MB", m_currentGPUMem / 1024 / 1024, m_budgetGPUMem / 1024 / 1024));
    for (auto [e, txt] : ctx.world.view<const DebugText, Ready>().each())
    {
        ctx.submissionQueue.Add<CmdDrawDebugText>(GameViewType::Overlay, txt.text);
    }
}
}  // namespace game::view
