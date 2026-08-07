#include "build_config.h"
#include "game/client.hpp"
#include "game/client_scene.hpp"
#include "game/client_scene2.hpp"
#include "game/debug_scene.hpp"
#include "game/minimal_scene.hpp"
#include "game/scene_ext.hpp"
#include "oge/platform/sdl3/create_window.hpp"
#include "oge/platform/window.hpp"
#include "oge/platform/window_app.hpp"

using namespace oge::platform::sdl3;

int main(int argc, char* argv[])
{
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
    app.RegisterScene<game::ClientConnScene>();
    app.RegisterScene<game::ClientScene2>();
    app.RegisterScene<game::DebugScene>();

    // app.SwitchToScene<game::DebugScene3>();
    app.SwitchToScene<game::ClientConnScene>(
        {{"next_scene", app.Id<game::ClientScene2>()}});
    window->Run(app);
    return 0;
}
