#include <memory_resource>
#include <string>
#include <string_view>
#include "game/components.hpp"
#include "game/sim/subsystem.hpp"
#include "oge/log.hpp"
#include "oge/platform/perf.hpp"
#include "build_config.h"

namespace game::sim
{
static void onLog(oge::LogLevel lvl, std::string_view msg, void* user)
{
    GameState* ctx = reinterpret_cast<GameState*>(user);
    auto e = ctx->world.create();
    ctx->world.emplace<DebugText>(e, std::pmr::string{msg, ctx->memory.multiFrameBuffer.Resource()}, 5.f);
}

void SubsystemDebugText::onAttach(GameState& ctx)
{
    frameCount = 0;
    accumTime = 0.f;
    currentFrameTime = 0.f;
    perfStatus = {};
    totalPerfStatus = {};

    oge::GetLogger()->SetSink(onLog, &ctx);
}

void SubsystemDebugText::onDetach(GameState& ctx) {}

void SubsystemDebugText::onUpdate(FGameState& ctx)
{
    using namespace oge::platform;
    for (auto [e, txt] : ctx.world.view<DebugText>()->each())
    {
        txt.remainingTime -= ctx.dt;
        if (txt.remainingTime <= 0.f)
            ctx.world.destroy(e);
    }

    ++frameCount;
    accumTime += ctx.dt;
    totalPerfStatus = totalPerfStatus + ctx.world.ctx().get<FramePerfStatus>();

    if (accumTime >= 1.f)
    {
        perfStatus = totalPerfStatus / frameCount;
        totalPerfStatus = {};

        accumTime = 0;
        frameCount = 0;
    }
    auto entity = ctx.world.create();
    auto& txt = ctx.world.emplace<DebugText>(entity, std::pmr::string{std::pmr::new_delete_resource()});
    fmt::format_to(std::back_inserter(txt.text),
                   "{}\n{:.2f} ms | I {:.2f} | L {:.2f} | U {:.2f} | S {:.2f}\nCPU: {:.2f}%\nMEM: {} MB {} KB",
                   BUILD_TAG,
                   perfStatus.actualFrameTime(), perfStatus.inputProcessingTime, perfStatus.logicTime,
                   perfStatus.assetUploadTime, perfStatus.renderSubmitTime, perfStatus.cpuUsage,
                   GetRAMUsage() / 1024 / 1024, (GetRAMUsage() / 1024) % 1024);

}
}  // namespace game::sim
