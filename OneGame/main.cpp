#include "Engine/GameApp.hpp"
#include "Engine/Platform/IGameWindow.hpp"

using namespace OneGame::Engine;
using namespace OneGame::Engine::Graphics;


int main(int argc, char* argv[]) {
	(void)argc;
	(void)argv;

	auto window = CreateGameWindow("OneGame", 1280, 720);
	auto backend = CreateBackend(BackendType::Vulkan);
	auto app = GameClientApp(*backend);
	window->Run(app);
}
