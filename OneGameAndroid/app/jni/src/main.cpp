#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "Engine/Platform/IGameWindow.hpp"
#include "Engine/GameApp.hpp"

/* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */
/*                                                       */
/* Remove this source, and replace with your SDL sources */
/*                                                       */
/* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */

using namespace OneGame::Engine;
using namespace OneGame::Engine::Graphics;

extern "C" {
	int main(int argc, char* argv[]) {
		(void)argc;
		(void)argv;

		SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");

		auto window = CreateGameWindow("OneGame", 0, 0);
		auto app = GameApp();

		window->Run(app);
		return 0;
	}
}
