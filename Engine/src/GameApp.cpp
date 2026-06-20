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
	constexpr uint32_t DEBUG_SCENE2 = 1;

	GameApp::GameApp() : assetManager(asyncDispatcher, jobSystem)
	{
		allScenes.push_back(std::unique_ptr<IScene>(new DebugScene()));
		allScenes.push_back(std::unique_ptr<IScene>(new DebugScene2()));
		TransferToScene(DEBUG_SCENE2);
	}

	GameApp::~GameApp() = default;

	void GameApp::Initialize(WindowHandle* handle)
	{
		backend = CreateBackend(BackendType::Vulkan);
		backend->Initialize(BackendDesc{ handle, FrameTimePreference::Unlimited });
		LOG_DEBUG("Backend created");
		streamingManager.Initialize(backend.get());
		LOG_DEBUG("StreamingManager created");
		renderer.Initialize(backend.get(), assetManager, streamingManager);
		LOG_DEBUG("Renderer created");

		for (auto& scene : allScenes)
		{
			auto assetBundle = AssetBundleWriter<UploadType::Immediate>(&assetManager, &streamingManager, backend.get());
			AppInitContext appInitCtx
			{
				backend.get(),
				world,
				assetBundle,
				dispatcher,
			};
			scene->Initialize(appInitCtx);
		}
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

		std::vector<SceneAction> outSceneActions;
		auto interSceneBundle = AssetBundleWriter<UploadType::Async>(&assetManager, &streamingManager, backend.get());
		AppContext appCtx
		{
			backend.get(),
			world,
			interSceneBundle,
			dispatcher,
			input,
			outSceneActions,
		};
		if (nextScene != nullptr)
		{
			if (currentScene != nullptr)
			{
				currentScene->Exit(appCtx);
				interSceneBundle = AssetBundleWriter<UploadType::Async>(&assetManager, &streamingManager, backend.get());
				appCtx.assets = interSceneBundle;
			}
			nextScene->Enter(appCtx);
			interSceneBundle = AssetBundleWriter<UploadType::Async>(&assetManager, &streamingManager, backend.get());
			appCtx.assets = interSceneBundle;
			currentScene = nextScene;
			nextScene = nullptr;
		}
		assert(currentScene != nullptr);
		currentScene->Update(appCtx, dt);

		renderer.Prepare(&world, backend.get(), dt);
		auto tcmd = backend->CreateCommandList(QueueType::Transfer);
		tcmd->Begin();
		streamingManager.RunUploadStep(backend.get(), tcmd.get());
		tcmd->End();

		renderer.Render(backend.get(), dt);
		auto endRes = backend->EndFrame();
		world.clear();

		for (auto& action : appCtx.outSceneActions)
		{
			if (action.type == SceneActionType::SetMouseWarpping)
			{
				if (action.setMouseWarpping.enabled)
				{
					appRes = appRes | AppFrameAction::WrapMouse;
				}
				else
				{
					appRes = appRes | AppFrameAction::UnwrapMouse;
				}
			}
		}

		if (endRes == EndFrameAction::RecreateSurface)
			return appRes | AppFrameAction::WaitSurface;

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