#include "Engine/ECS/ISubsystem.hpp"
#include "Engine/Math.hpp"
#include "Engine/Graphics/PresentationObjects.hpp"
#include <format>


namespace OneGame::Engine::ECS
{
	void SubsystemDebugInfo::Initialize(SubsystemInitContext& ctx)
	{
		gpuInfo = ctx.ctx.backend.GetGPUInfo();
	}

	void SubsystemDebugInfo::Update(SubsystemContext& ctx)
	{
		memUsage = ctx.backend.GetGPUMemoryUsage();
		++frameCount;
		accumTime += ctx.dt;

		if (accumTime >= 1.f)
		{
			currentFrameTime = accumTime / frameCount;
			currentFPS = 1 / currentFrameTime;
			currentFrameTime *= 1000.f;
			accumTime = 0;
			frameCount = 0;
		}
	}

	void SubsystemDebugInfo::Present(entt::registry& world)
	{
		auto gpuInfoEntity = world.create();
		auto& gpuInfoText = world.emplace<Graphics::PDebugText>(gpuInfoEntity);
		gpuInfoText.text = gpuInfo.name;
		auto debugInfoEntity = world.create();
		world.emplace<Graphics::PDebugText>(debugInfoEntity);
		world.get<Graphics::PDebugText>(debugInfoEntity).text = std::format("FPS {} ({} ms)\nGPU Heap 0: {} MB / {} MB\nGPU Heap 1: {} MB / {} MB", currentFPS, currentFrameTime, memUsage.heapUsage[0] / 1024 / 1024, memUsage.heapBudget[0] / 1024 / 1024, memUsage.heapUsage[1] / 1024 / 1024, memUsage.heapBudget[1] / 1024 / 1024);
	}
}
