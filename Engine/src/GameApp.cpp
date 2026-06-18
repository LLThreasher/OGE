#include "Engine/GameApp.hpp"
#include "Engine/Graphics/IGraphicsBackend.hpp"
#include "Engine/Graphics/DebugPass.hpp"
#include "Engine/Scenes/DebugScene.hpp"

#define LOGGER_NAME "Engine"
#include "Engine/Logger.hpp"

#define TEST_TERRAIN_0

namespace OneGame::Engine
{
	using namespace Graphics;

	constexpr uint32_t DEBUG_SCENE = 0;

	GameApp::GameApp()
	{
		allScenes.push_back(std::unique_ptr<IScene>(new DebugScene()));
	}

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

		for (auto& scene : allScenes)
		{
			auto assetBundle = assetManager.CreateAssetBundle(&streamingManager, backend.get());
			AppInitContext appInitCtx
			{
				world,
				renderer,
				*assetBundle.get(),
				dispatcher,
			};
			scene->Initialize(appInitCtx);
		}
		TransferToScene(DEBUG_SCENE);
	}

	void GameApp::TransferToScene(uint32_t scene)
	{
		assert(scene < allScenes.size());
		nextScene = allScenes[scene].get();
	}

	void GameApp::Shutdown()
	{
		backend->WaitDeviceIdle();
		renderer.Shutdown(backend.get());
		streamingManager.Shutdown(backend.get());
		backend->Shutdown();
	}

	AppFrameAction GameApp::Update(float dt, InputSystem& input)
	{
		AppFrameAction appRes = AppFrameAction::None;
		auto res = backend->BeginFrame();
		if (res == BeginFrameAction::RecreateSurface)
			return appRes | AppFrameAction::WaitSurface;
		if (res != BeginFrameAction::Continue)
			return appRes | AppFrameAction::None;

		if (accumTime >= 1.f)
		{
			currentFPS = 1 / (accumTime / frameCount);
			accumTime = 0;
			frameCount = 0;
		}
		auto memUsage = backend->GetGPUMemoryUsage();
		world.get<DebugInfoPass::ComponentDebugText>(debugInfoEntity).text = std::format("FPS {}\nGPU Heap 0: {} MB / {} MB\nGPU Heap 1: {} MB / {} MB", currentFPS, memUsage.heapUsage[0] / 1024 / 1024, memUsage.heapBudget[0] / 1024 / 1024, memUsage.heapUsage[1] / 1024 / 1024, memUsage.heapBudget[1] / 1024 / 1024);

		std::vector<SceneAction> outSceneActions;
		auto interSceneBundle = assetManager.CreateAssetBundleAsync(&streamingManager, backend.get());
		AppContext appCtx
		{
			world,
			renderer,
			interSceneBundle.get(),
			dispatcher,
			input,
			outSceneActions,
		};
		if (nextScene != nullptr)
		{
			if (currentScene != nullptr)
			{
				currentScene->Exit(appCtx);
				interSceneBundle = assetManager.CreateAssetBundleAsync(&streamingManager, backend.get());
				appCtx.assets = interSceneBundle.get();
			}
			nextScene->Enter(appCtx);
			interSceneBundle = assetManager.CreateAssetBundleAsync(&streamingManager, backend.get());
			appCtx.assets = interSceneBundle.get();
			currentScene = nextScene;
			nextScene = nullptr;
		}
		assert(currentScene != nullptr);
		currentScene->Update(appCtx, dt);

		renderer.Prepare(&world, backend.get(), dt);
		auto tcmd = backend->CreateCommandList(QueueType::Transfer);
		tcmd->Begin();
		streamingManager.RunUploadStep(backend.get(), tcmd.get(), &dispatcher);
		tcmd->End();

		renderer.Render(backend.get(), dt);
		auto endRes = backend->EndFrame();
		if (endRes == EndFrameAction::RecreateSurface)
			return appRes | AppFrameAction::WaitSurface;

		++frameCount;
		accumTime += dt;

		return appRes;
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