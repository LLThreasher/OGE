#include "Engine/GameApp.hpp"
#include "Engine/Graphics/IGraphicsBackend.hpp"
#include "Engine/Graphics/DebugPass.hpp"

#define LOGGER_NAME "Engine"
#include "Engine/Logger.hpp"

#define TEST_TERRAIN_0

namespace OneGame::Engine
{
	using namespace Graphics;

	GameApp::~GameApp() = default;

	void GameApp::Initialize(WindowHandle* handle)
	{
		backend = CreateBackend(BackendType::Vulkan);
		backend->Initialize(BackendDesc{ handle, FrameTimePreference::VSync });
		LOG_DEBUG("Backend created");
		streamingManager.Initialize(backend.get());
		LOG_DEBUG("StreamingManager created");
		renderer.Initialize(backend.get(), assetManager, streamingManager);
		LOG_DEBUG("Renderer created");
		gpuInfo = backend->GetGPUInfo();
		auto gpuInfoEntity = world.create();
		auto& gpuInfoText = world.emplace<DebugInfoPass::ComponentDebugText>(gpuInfoEntity);
		gpuInfoText.text = gpuInfo.name;
		debugInfoEntity = world.create();
		world.emplace<DebugInfoPass::ComponentDebugText>(debugInfoEntity);

#ifdef TEST_TERRAIN_0
		auto chunkEntity = world.create();
		world.emplace<Terrain::ActiveChunkTag>(chunkEntity);
		Terrain::ChunkMesh cm { 0, 0, 0, { 0, 36 } };
		world.emplace<Terrain::ChunkMesh>(chunkEntity, cm);
#endif
	}

	void GameApp::Shutdown()
	{
		backend->WaitDeviceIdle();
		renderer.Shutdown(backend.get());
		streamingManager.Shutdown(backend.get());
		backend->Shutdown();
	}

	AppFrameAction GameApp::Update(float dt)
	{
		auto res = backend->BeginFrame();
		if (res == BeginFrameAction::RecreateSurface)
			return AppFrameAction::RecreateSufrace;
		if (res != BeginFrameAction::Continue)
			return AppFrameAction::Continue;

		if (accumTime >= 1.f)
		{
			currentFPS = 1 / (accumTime / frameCount);
			accumTime = 0;
			frameCount = 0;
		}
		auto memUsage = backend->GetGPUMemoryUsage();
		world.get<DebugInfoPass::ComponentDebugText>(debugInfoEntity).text = std::format("FPS {}\nGPU Heap 0: {} MB / {} MB\nGPU Heap 1: {} MB / {} MB", currentFPS, memUsage.heapUsage[0] / 1024 / 1024, memUsage.heapBudget[0] / 1024 / 1024, memUsage.heapUsage[1] / 1024 / 1024, memUsage.heapBudget[1] / 1024 / 1024);

		renderer.Prepare(&world, backend.get());
		auto tcmd = backend->CreateCommandList(QueueType::Transfer);
		tcmd->Begin();
		streamingManager.RunUploadStep(backend.get(), tcmd.get(), &dispatcher);
		tcmd->End();
		renderer.Render(backend.get(), dt);
		auto endRes = backend->EndFrame();
		if (endRes == EndFrameAction::RecreateSurface)
			return AppFrameAction::RecreateSufrace;

		++frameCount;
		accumTime += dt;

		return AppFrameAction::Continue;
	}

	void GameApp::OnResize(int width, int height)
	{
		backend->Resize(width, height);
	}

	void GameApp::OnWindowRecreate(Graphics::WindowHandle* handle)
	{
		backend->RecreateSurface(handle);
	}
}