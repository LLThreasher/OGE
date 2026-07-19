#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "oge/platform/window.hpp"
#include "oge/platform/window_app.hpp"
#include "oge/platform/sdl3/create_window.hpp"
#include "game/client.hpp"
#include "game/debug_scene.hpp"

using namespace oge::platform::sdl3;

extern "C" {
int main(int argc, char* argv[]) {
	(void)argc;
	(void)argv;

	auto window = CreateSDL3Window("OneGame", 1280, 720);
	auto app = game::Client();
	app.RegisterScene<game::DebugScene3>();
	app.SwitchToScene<game::DebugScene3>();
	window->Run(app);
	// app.RegisterScene<DebugScene3>();
	// app.SwitchToScene<DebugScene3>();
	// app.RegisterScene<DebugClient>();
	// app.SwitchToScene<DebugClient>();
	// window->Run(app);
	return 0;
}
}
