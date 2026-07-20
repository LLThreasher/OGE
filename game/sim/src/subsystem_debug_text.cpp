#include "game/components.hpp"
#include "game/sim/subsystem.hpp"
#include "oge/platform/perf.hpp"

namespace game::sim
{
void SubsystemDebugText::onAttach(GameState& ctx)
{
    frameCount = 0;
    accumTime = 0.f;
    currentFPS = 0.f;
    currentFrameTime = 0.f;
    perfStatus = {};
    totalPerfStatus = {};
}

void SubsystemDebugText::onDetach(GameState& ctx) {}

void SubsystemDebugText::onUpdate(FGameState& ctx)
{
    using namespace oge::platform;
    for (auto e : ctx.world.view<const DebugText>())
    {
        if (ctx.world.all_of<Ready>(e)) ctx.world.destroy(e);
    }

    ++frameCount;
    accumTime += ctx.dt;
    totalPerfStatus = totalPerfStatus + ctx.world.ctx().get<FramePerfStatus>();

    if (accumTime >= 1.f)
    {
        perfStatus = totalPerfStatus / frameCount;
        totalPerfStatus = {};

        currentFrameTime = accumTime / frameCount;
        currentFPS = 1 / currentFrameTime;
        currentFrameTime *= 1000.f;
        accumTime = 0;
        frameCount = 0;
    }
    static std::string buffer;
    buffer.clear();
    std::format_to(std::back_inserter(buffer),
                   "FPS {:.2f} ({:.2f} ms | I {:.2f} | L {:.2f} | U {:.2f} | S {:.2f})\nCPU: {:.2f}%\nMEM: {} MB {} KB",
                   currentFPS, perfStatus.actualFrameTime(), perfStatus.inputProcessingTime, perfStatus.logicTime,
                   perfStatus.assetUploadTime, perfStatus.renderSubmitTime, perfStatus.cpuUsage,
                   GetRAMUsage() / 1024 / 1024, (GetRAMUsage() / 1024) % 1024);

    auto entity = ctx.world.create();
    ctx.world.emplace<DebugText>(entity, buffer);
    ctx.world.emplace<Ready>(entity);
}
}  // namespace game::sim
