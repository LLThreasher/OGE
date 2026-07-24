#include "oge/platform/window.hpp"
#include "oge/platform/window_app.hpp"
#include "oge/platform/sdl3/create_window.hpp"
#include "game/client.hpp"
#include "game/debug_scene.hpp"
#include "build_config.h"

using namespace oge::platform::sdl3;

int main(int argc, char* argv[]) {
	(void)argc;
	(void)argv;

    // std::pmr::set_default_resource(std::pmr::null_memory_resource());
	#ifdef OGE_DEBUG
	std::string app_name = fmt::format("{}_{}", APP_NAME, BUILD_TAG);
	#else
	std::string app_name = fmt::format("{} {}", APP_NAME, MARKETING_VERSION);
	#endif
	auto window = CreateSDL3Window(app_name, 1280, 720);
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
