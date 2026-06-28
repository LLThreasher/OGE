#include <format>

#include "Engine/ECS/ISubsystem.hpp"
#include "Engine/Graphics/PresentationObjects.hpp"
#include "Engine/Math.hpp"

namespace OneGame::Engine::ECS
{
void SubsystemDebugInfo::Initialize(AppContext& ctx, entt::registry& gameWorld) {}

void SubsystemDebugInfo::Update(AppContext& ctx, entt::registry& gameWorld, const FrameInputData& fd)
{
    ++frameCount;
    accumTime += fd.dt;

    if (accumTime >= 1.f)
    {
        currentFrameTime = accumTime / frameCount;
        currentFPS = 1 / currentFrameTime;
        currentFrameTime *= 1000.f;
        accumTime = 0;
        frameCount = 0;
    }
}

void SubsystemDebugInfo::Present(const entt::registry& gameWorld, PresentationContext& ctx, FrameOutputData& fd)
{
    auto gpuInfo = ctx.backend.GetGPUInfo();
    auto memUsage = ctx.backend.GetGPUMemoryUsage();

    auto& world = fd.presentationWorld;
    auto gpuInfoEntity = world.create();
    auto& gpuInfoText = world.emplace<Graphics::PDebugText>(gpuInfoEntity);
    gpuInfoText.text = gpuInfo.name;
    auto debugInfoEntity = world.create();
    world.emplace<Graphics::PDebugText>(debugInfoEntity);
    world.get<Graphics::PDebugText>(debugInfoEntity).text =
        std::format("FPS {:.2f} ({:.2f} ms)\nGPU Heap 0: {} MB / {} MB", currentFPS, currentFrameTime,
                    memUsage.heapUsage[0] / 1024 / 1024, memUsage.heapBudget[0] / 1024 / 1024);
}
}  // namespace OneGame::Engine::ECS
