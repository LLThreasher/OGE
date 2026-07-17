#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "Engine/Platform/IGameWindow.hpp"
#include "Engine/WindowedGameApp.hpp"

#include "OneGame/DebugScene.hpp"
#include "OneGame/Client.hpp"

using namespace OneGame;
using namespace OneGame::Engine;
using namespace OneGame::Engine::Graphics;

extern "C" {
	int main(int argc, char* argv[]) {
		(void)argc;
		(void)argv;

		SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");

		auto window = CreateGameWindow("OneGame", 0, 0);
        auto backend = CreateBackend(BackendType::Vulkan);
        auto app = GameGraphicApp(*backend);
		app.RegisterScene<DebugScene3>();
		app.SwitchToScene<DebugScene3>();
		window->Run(app);
		return 0;
	}
}
