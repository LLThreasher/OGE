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

	constexpr uint32_t EMPTY_SCENE		= 0;
	constexpr uint32_t DEBUG_SCENE		= 1;
	constexpr uint32_t DEBUG_SCENE2		= 2;

	GameApp::GameApp()
	{
		allScenes.push_back(std::unique_ptr<IScene>(new EmptyScene()));
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
		streamingManager.Initialize(*backend);
		LOG_DEBUG("StreamingManager created");
		renderer.Initialize(*backend, assetManager, streamingManager);
		LOG_DEBUG("Renderer created");

		AppContext appCtx
		{
			*backend,
			assetManager,
			streamingManager,
			renderer,
			dispatcher,
		};

		AppContext ctx
		{
			*backend,
			assetManager,
			streamingManager,
			renderer,
			dispatcher,
		};

		for (auto& scene : allScenes)
		{
			auto assetBundle = AssetBundleWriter(assetManager, streamingManager, *backend);
			scene->Initialize(ctx);
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
		renderer.Shutdown(*backend);
		streamingManager.Shutdown(*backend);
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

		AppContext appCtx
		{
			*backend,
			assetManager,
			streamingManager,
			renderer,
			dispatcher,
		};
		std::vector<SceneAction> outSceneActions;
		FrameContext frame
		{
			dt,
			world,
			input,
			outSceneActions,
		};
		if (nextScene != nullptr)
		{
			if (currentScene != nullptr)
			{
				currentScene->Exit(appCtx);
			}
			nextScene->Enter(appCtx);
			currentScene = nextScene;
			nextScene = nullptr;
		}
		assert(currentScene != nullptr);
		world.clear();
		currentScene->Update(appCtx, frame);

		renderer.Prepare(*backend, world, dt);
		auto& tcmd = backend->CreateCommandList(QueueType::Transfer);
		tcmd.Begin();
		streamingManager.RunUploadStep(*backend, tcmd);
		tcmd.End();

		renderer.Render(*backend, dt);
		auto endRes = backend->EndFrame();

		for (auto& action : frame.outSceneActions)
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