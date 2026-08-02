#include <memory_resource>
#include <string>
#include <string_view>

#include "build_config.h"
#include "game/components.hpp"
#include "game/sim/subsystem.hpp"
#include "oge/handle.hpp"
#include "oge/log.hpp"
#include "oge/platform/perf.hpp"
#include "oge/pool.hpp"

namespace game::sim
{
static void onLog(oge::LogLevel lvl, std::string_view msg, void* user)
{
    GameState* ctx = reinterpret_cast<GameState*>(user);
    auto& storage = ctx->world.ctx().get<oge::Pool<0, DebugText>>();
    storage.Create(std::move(std::pmr::string{
                       msg, ctx->memory.multiFrameBuffer.Resource()}),
                   5.f);
}

void SubsystemDebugText::onAttach(GameState& ctx)
{
    frameCount = 0;
    accumTime = 0.f;
    currentFrameTime = 0.f;
    perfStatus = {};
    totalPerfStatus = {};

    ctx.world.ctx().emplace<oge::Pool<0, DebugText>>();
    oge::GetLogger()->SetSink(onLog, &ctx);
}

void SubsystemDebugText::onDetach(GameState& ctx)
{
    oge::GetLogger()->ClearSink();
}

void SubsystemDebugText::onUpdate(FGameState& ctx)
{
    using namespace oge::platform;
    auto& pool = ctx.world.ctx().get<oge::Pool<0, DebugText>>();
    for (oge::Handle<0> cursor{}; auto txt = pool.Poll(cursor);)
    {
        txt->remainingTime -= ctx.dt;
        if (txt->remainingTime <= 0.f) pool.Destroy(cursor);
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
        ramInfo = GetRAMUsage();
        cpuUsage = GetCPUUsage();
    }
    auto handle = pool.Create(std::move(std::pmr::string{ctx.memory.fixedUpdateBuffer.Resource()}));
    auto txt = pool.Get(handle);
    fmt::format_to(std::back_inserter(txt->text),
                   "{}\n{:.2f} ms | I {:.2f} | L {:.2f} | U {:.2f} | S "
                   "{:.2f}\nCPU: {:.2f}%\nMEM: {} MB | NB {} MB",
                   BUILD_TAG, perfStatus.actualFrameTime(),
                   perfStatus.inputProcessingTime, perfStatus.logicTime,
                   perfStatus.assetUploadTime, perfStatus.renderSubmitTime,
                   cpuUsage, ramInfo.RSS / 1024 / 1024,
                   ramInfo.NativeHeapBlks / 1024 / 1024);
}
}  // namespace game::sim
