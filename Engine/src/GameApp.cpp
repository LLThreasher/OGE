#include "Engine/GameApp.hpp"
#include "Engine/Graphics/IGraphicsBackend.hpp"
#include "Engine/Graphics/DebugPass.hpp"

#define LOGGER_NAME "Engine"
#include "Engine/Logger.hpp"

namespace OneGame::Engine
{
	using namespace Graphics;

	GameApp::~GameApp() = default;

	void GameApp::Initialize(WindowHandle* handle)
	{
		backend = CreateBackend(BackendType::Vulkan);
		backend->Initialize(BackendDesc{ handle, FrameTimePreference::VSync });
		renderer.Initialize(backend.get(), assetManager);
		gpuInfo = backend->GetGPUInfo();
		auto gpuInfoEntity = world.create();
		auto& gpuInfoText = world.emplace<DebugInfoPass::ComponentDebugText>(gpuInfoEntity);
		gpuInfoText.text = gpuInfo.name;
		debugInfoEntity = world.create();
		world.emplace<DebugInfoPass::ComponentDebugText>(debugInfoEntity);
	}

	void GameApp::Shutdown()
	{
		renderer.Shutdown(backend.get());
		backend->Shutdown();
	}

	void GameApp::Update(float dt)
	{
		if (accumTime >= 1.f)
		{
			currentFPS = 1 / (accumTime / frameCount);
			accumTime = 0;
			frameCount = 0;
		}
		auto memUsage = backend->GetGPUMemoryUsage();
		world.get<DebugInfoPass::ComponentDebugText>(debugInfoEntity).text = std::format("FPS {}\nGPU Heap 0: {} MB / {} MB\nGPU Heap 1: {} MB / {} MB", currentFPS, memUsage.heapUsage[0] / 1024 / 1024, memUsage.heapBudget[0] / 1024 / 1024, memUsage.heapUsage[1] / 1024 / 1024, memUsage.heapBudget[1] / 1024 / 1024);

		backend->BeginFrame();
		renderer.Prepare(&world);
		auto tcmd = backend->CreateCommandList(QueueType::Transfer);
		tcmd->Begin();
		assetManager.StageUpload(backend.get(), renderer.GetStagingBuffer(), tcmd.get());
		tcmd->End();
		renderer.Render(backend.get(), dt);
		backend->EndFrame();
		++frameCount;
		accumTime += dt;
	}

	void GameApp::OnResize(int width, int height)
	{
		backend->Resize(width, height);
	}
}