#include <format>
#include <iterator>

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
    debugString.reserve(256);
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

    debugString.clear();
    for (auto [e, txt] : ctx.world.view<const DebugText>().each())
    {
        debugString.append(txt.text);
        debugString.push_back('\n');
    }

    if (tickScheduler.Poll(ctx.dt))
    {
        tickScheduler.ConsumeTick();
        gpuDebugString.clear();
        gpuDebugString.append(gpuInfo.name);
        gpuDebugString.push_back('\n');
        std::format_to(std::back_inserter(gpuDebugString), "FPS {:.2f}\n", 1.f / ctx.dt);
        std::format_to(std::back_inserter(gpuDebugString), "GPU MEM: {} / {} MB", m_currentGPUMem / 1024 / 1024,
                       m_budgetGPUMem / 1024 / 1024);
    }
    
    uint8_t offsetX = 40;
    uint8_t offsetY = 10;
    debugString.append(gpuDebugString);
    debugFont->CreateTextSprites(spriteQueue, {debugString, 32},
                                 {{{offsetX, offsetY}, ctx.assets.backend.SwapchainExtent() - oge::U16Point2{offsetX, offsetY}}});
}
}  // namespace game::view
