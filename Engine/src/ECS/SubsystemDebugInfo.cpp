#include <format>

#include "Engine/ECS/ISubsystem.hpp"
#include "Engine/Graphics/PresentationObjects.hpp"
#include "Engine/Math.hpp"

namespace OneGame::Engine::ECS
{
void SubsystemDebugInfo::Initialize(GameWorldContext& game, AppContext ctx)
{
    frameCount = 0;
    accumTime = 0.f;
    currentFPS = 0.f;
    currentFrameTime = 0.f;
    perfStatus = {};
    totalPerfStatus = {};
}

void SubsystemDebugInfo::Update(GameWorldContext& game, AppContext ctx, const FrameInputData& fd)
{
    ++frameCount;
    accumTime += fd.dt;
    totalPerfStatus = totalPerfStatus + fd.perfStats;

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
}

void SubsystemDebugInfo::Present(const GameWorldContext& game, PresentationContext ctx, FrameOutputData& fd)
{
    auto gpuInfo = ctx.backend.GetGPUInfo();
    auto memUsage = ctx.backend.GetGPUMemoryUsage();

    auto& world = fd.presentationWorld;
    AddDebugInfo(world, gpuInfo.name);
    AddDebugInfo(world, std::format("FPS {:.2f} ({:.2f} ms | {:.2f} | {:.2f} | {:.2f} | {:.2f})\nGPU Heap 0: {} MB / {} MB", currentFPS, perfStatus.actualFrameTime(), perfStatus.inputProcessingTime, perfStatus.logicTime, perfStatus.assetUploadTime, perfStatus.renderSubmitTime,
                             memUsage.heapUsage[0] / 1024 / 1024, memUsage.heapBudget[0] / 1024 / 1024));
}

void AddDebugInfo(entt::registry& world, std::string_view msg)
{
    auto e = world.create();
    world.emplace<Graphics::PDebugText>(e).text = msg;
}
}  // namespace OneGame::Engine::ECS
