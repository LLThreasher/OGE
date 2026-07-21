#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "game/client.hpp"
#include "game/debug_scene.hpp"
#include "game/debug_scene2.hpp"
#include "oge/platform/sdl3/create_window.hpp"
#include "oge/platform/window.hpp"
#include "oge/platform/window_app.hpp"

using namespace oge::platform::sdl3;

extern "C"
{
    int main(int argc, char* argv[])
    {
        (void)argc;
        (void)argv;

        SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");

        auto window = CreateSDL3Window("OneGame", 0, 0);
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
