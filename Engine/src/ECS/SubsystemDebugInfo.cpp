#include "Engine/ECS/ISubsystem.hpp"
#include "Engine/Math.hpp"
#include "Engine/Graphics/PresentationObjects.hpp"
#include <format>


namespace OneGame::Engine::ECS
{
	void SubsystemDebugInfo::Initialize(SubsystemInitContext& ctx)
	{
		gpuInfo = ctx.backend->GetGPUInfo();
	}

	void SubsystemDebugInfo::Update(SubsystemContext& ctx, float dt)
	{
		memUsage = ctx.backend->GetGPUMemoryUsage();
		++frameCount;
		accumTime += dt;

		if (accumTime >= 1.f)
		{
			currentFPS = 1 / (accumTime / frameCount);
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
		world.get<Graphics::PDebugText>(debugInfoEntity).text = std::format("FPS {}\nGPU Heap 0: {} MB / {} MB\nGPU Heap 1: {} MB / {} MB", currentFPS, memUsage.heapUsage[0] / 1024 / 1024, memUsage.heapBudget[0] / 1024 / 1024, memUsage.heapUsage[1] / 1024 / 1024, memUsage.heapBudget[1] / 1024 / 1024);
	}
}
