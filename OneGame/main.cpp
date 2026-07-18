#include "Engine/WindowedGameApp.hpp"
#include "Engine/Platform/IGameWindow.hpp"
#include "Engine/Graphics/IGraphicsBackend.hpp"

#include "OneGame/DebugScene.hpp"
#include "OneGame/Client.hpp"

using namespace OneGame;
using namespace OneGame::Engine;
using namespace OneGame::Engine::Graphics;


int main(int argc, char* argv[]) {
	(void)argc;
	(void)argv;

	auto window = CreateGameWindow("OneGame", 1280, 720);
	auto backend = CreateBackend(BackendType::Vulkan);
	auto app = GameGraphicApp(*backend);
	app.RegisterScene<DebugScene>();
	app.SwitchToScene<DebugScene>();
	// app.RegisterScene<DebugScene3>();
	// app.SwitchToScene<DebugScene3>();
	// app.RegisterScene<DebugClient>();
	// app.SwitchToScene<DebugClient>();
	window->Run(app);
	return 0;
}
