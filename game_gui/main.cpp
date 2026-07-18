#include "oge/platform/window.hpp"
#include "oge/platform/window_app.hpp"
#include "oge/platform/sdl3/create_window.hpp"
#include "game/client.hpp"

using namespace oge::platform::sdl3;

int main(int argc, char* argv[]) {
	(void)argc;
	(void)argv;

	auto window = CreateSDL3Window("OneGame", 1280, 720);
	auto app = game::Client();
	window->Run(app);
	// auto backend = CreateBackend(BackendType::Vulkan);
	// auto app = GameGraphicApp(*backend);
	// app.RegisterScene<DebugScene3>();
	// app.SwitchToScene<DebugScene3>();
	// app.RegisterScene<DebugClient>();
	// app.SwitchToScene<DebugClient>();
	// window->Run(app);
	return 0;
}
