#include "build_config.h"
#include "game/client.hpp"
#include "game/client_conn_scene.hpp"
#include "game/client_scene2.hpp"
#include "game/debug_voxel_view.hpp"
#include "game/debug_view.hpp"
#include "game/scene.hpp"
#include "game/scene_ext.hpp"
#include "oge/log.hpp"
#include "oge/platform/sdl3/create_window.hpp"
#include "oge/platform/window.hpp"
#include "oge/platform/window_app.hpp"

#include <stdexcept>
#include <string>

using namespace oge::platform::sdl3;

int main(int argc, char* argv[])
{
    // Scene to start with: argv[1] as short ("DebugScene3") or full
    // ("core::DebugScene3") type name.  Defaults to DebugScene3.
    std::string sceneName = "core::DebugVoxelView<" "core::Scene" ">";
    if (argc > 1)
    {
        std::string arg = argv[1];
        if (arg.find("::") == std::string::npos)
        {
            arg = "core::DebugView<" "core::" + arg + ">";
        }
        sceneName = arg;
    }

    // std::pmr::set_default_resource(std::pmr::null_memory_resource());
#ifdef OGE_DEBUG
    std::string app_name = fmt::format("{}_{}", APP_NAME, BUILD_TAG);
#else
    std::string app_name = fmt::format("{} {}", APP_NAME, MARKETING_VERSION);
#endif
    auto window = CreateSDL3Window(app_name, 1280, 720);
    auto app = game::Client();

    app.AF().RegisterABC<game::Scene>();
    app.AF().RegisterDerived<game::Scene, game::Scene>();
    app.AF().RegisterDerived<game::Scene, game::ClientConnScene>();
    app.AF().RegisterDerived<game::Scene, game::ClientScene2>();

    app.RegisterScene<game::DebugView<game::Scene>>();
    app.RegisterScene<game::DebugView<game::ClientConnScene>>();

    app.RegisterScene<game::DebugVoxelView<game::Scene>>();
    app.RegisterScene<game::DebugVoxelView<game::ClientScene2>>();

    try
    {
        app.SwitchToScene(sceneName);
    }
    catch (const std::out_of_range&)
    {
        LOG_WARN("Unknown scene '{}', falling back to core::SceneExt", sceneName);
        app.SwitchToScene<game::SceneView>();
    }
    window->Run(app);
    return 0;
}
